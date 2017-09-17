/* This file is part of the KDE project
 * Copyright (C) 2008 Boudewijn Rempt <boud@valdyas.org>
 * Copyright (C) 2010 Lukáš Tvrdý <lukast.dev@gmail.com>
 * Copyright (C) 2011 Silvio Heinrich <plassy@web.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "widgets/kis_paintop_presets_popup.h"

#include <QList>
#include <QComboBox>
#include <QHBoxLayout>
#include <QToolButton>
#include <QGridLayout>
#include <QFont>
#include <QMenu>
#include <QAction>
#include <QShowEvent>
#include <QFontDatabase>
#include <QWidgetAction>

#include <kconfig.h>
#include <klocalizedstring.h>

#include <KoDockRegistry.h>

#include <kis_icon.h>
#include <kis_icon.h>
#include <brushengine/kis_paintop_preset.h>
#include <brushengine/kis_paintop_config_widget.h>
#include <kis_canvas_resource_provider.h>
#include <widgets/kis_preset_chooser.h>
#include <widgets/kis_preset_selector_strip.h>

#include <ui_wdgpaintopsettings.h>
#include <kis_node.h>
#include "kis_config.h"

#include "kis_resource_server_provider.h"
#include "kis_lod_availability_widget.h"

#include "kis_signal_auto_connection.h"
#include <kis_paintop_settings_update_proxy.h>

// ones from brush engine selector
#include <brushengine/kis_paintop_factory.h>




struct KisPaintOpPresetsPopup::Private
{

public:

    Ui_WdgPaintOpSettings uiWdgPaintOpPresetSettings;
    QGridLayout *layout;
    KisPaintOpConfigWidget *settingsWidget;
    QFont smallFont;
    KisCanvasResourceProvider *resourceProvider;
    KisFavoriteResourceManager *favoriteResManager;

    bool detached;
    bool ignoreHideEvents;
    QSize minimumSettingsWidgetSize;
    QRect detachedGeometry;

    KisSignalAutoConnectionsStore widgetConnections;
};

KisPaintOpPresetsPopup::KisPaintOpPresetsPopup(KisCanvasResourceProvider * resourceProvider,
                                               KisFavoriteResourceManager* favoriteResourceManager,
                                               KisPresetSaveWidget* savePresetWidget,
                                               QWidget * parent)
    : QWidget(parent)
    , m_d(new Private())
{
    setObjectName("KisPaintOpPresetsPopup");
    setFont(KoDockRegistry::dockFont());

    current_paintOpId = "";

    m_d->resourceProvider = resourceProvider;
    m_d->favoriteResManager = favoriteResourceManager;

    m_d->uiWdgPaintOpPresetSettings.setupUi(this);

    m_d->layout = new QGridLayout(m_d->uiWdgPaintOpPresetSettings.frmOptionWidgetContainer);
    m_d->layout->setSizeConstraint(QLayout::SetFixedSize);

    m_d->uiWdgPaintOpPresetSettings.scratchPad->setupScratchPad(resourceProvider, Qt::white);
    m_d->uiWdgPaintOpPresetSettings.scratchPad->setCutoutOverlayRect(QRect(25, 25, 200, 200));
    m_d->uiWdgPaintOpPresetSettings.paintPresetIcon->setIcon(KisIconUtils::loadIcon("krita_tool_freehand"));

    m_d->uiWdgPaintOpPresetSettings.fillLayer->setIcon(KisIconUtils::loadIcon("document-new"));
    m_d->uiWdgPaintOpPresetSettings.fillLayer->hide();
    m_d->uiWdgPaintOpPresetSettings.fillGradient->setIcon(KisIconUtils::loadIcon("krita_tool_gradient"));
    m_d->uiWdgPaintOpPresetSettings.fillSolid->setIcon(KisIconUtils::loadIcon("krita_tool_color_fill"));
    m_d->uiWdgPaintOpPresetSettings.eraseScratchPad->setIcon(KisIconUtils::loadIcon("edit-delete"));
    m_d->uiWdgPaintOpPresetSettings.reloadPresetButton->setIcon(KisIconUtils::loadIcon("updateColorize")); // refresh icon
    m_d->uiWdgPaintOpPresetSettings.renameBrushPresetButton->setIcon(KisIconUtils::loadIcon("dirty-preset")); // edit icon

    m_d->uiWdgPaintOpPresetSettings.dirtyPresetIndicatorButton->setIcon(KisIconUtils::loadIcon("warning"));
    m_d->uiWdgPaintOpPresetSettings.dirtyPresetIndicatorButton->setToolTip(i18n("The settings for this preset have changed from their default."));


    m_d->uiWdgPaintOpPresetSettings.reloadPresetButton->setToolTip(i18n("Reload the brush preset"));
    m_d->uiWdgPaintOpPresetSettings.renameBrushPresetButton->setToolTip(i18n("Rename the brush preset"));


    // overwrite existing preset and saving a new preset use the same dialog
    saveDialog = savePresetWidget;
    saveDialog->scratchPadSetup(resourceProvider);
    saveDialog->setFavoriteResourceManager(m_d->favoriteResManager); // this is needed when saving the preset
    saveDialog->hide();

    // the area on the brush editor for renaming the brush. make sure edit fields are hidden by default
    toggleBrushRenameUIActive(false);


    // DETAIL and THUMBNAIL view changer
    QMenu* menu = new QMenu(this);

    menu->setStyleSheet("margin: 6px");
    menu->addSection(i18n("Display"));

    QActionGroup *actionGroup = new QActionGroup(this);

    KisPresetChooser::ViewMode mode = (KisPresetChooser::ViewMode)KisConfig().presetChooserViewMode();

    QAction* action = menu->addAction(KisIconUtils::loadIcon("view-preview"), i18n("Thumbnails"), m_d->uiWdgPaintOpPresetSettings.presetWidget, SLOT(slotThumbnailMode()));
    action->setCheckable(true);
    action->setChecked(mode == KisPresetChooser::THUMBNAIL);
    action->setActionGroup(actionGroup);

    action = menu->addAction(KisIconUtils::loadIcon("view-list-details"), i18n("Details"), m_d->uiWdgPaintOpPresetSettings.presetWidget, SLOT(slotDetailMode()));
    action->setCheckable(true);
    action->setChecked(mode == KisPresetChooser::DETAIL);
    action->setActionGroup(actionGroup);


    // add horizontal slider for the icon size
    QSlider* iconSizeSlider = new QSlider(this);
    iconSizeSlider->setOrientation(Qt::Horizontal);
    iconSizeSlider->setRange(30, 80);
    iconSizeSlider->setValue(m_d->uiWdgPaintOpPresetSettings.presetWidget->iconSize());
    iconSizeSlider->setMinimumHeight(20);
    iconSizeSlider->setMinimumWidth(40);
    iconSizeSlider->setTickInterval(10);


    QWidgetAction *sliderAction= new QWidgetAction(this);
    sliderAction->setDefaultWidget(iconSizeSlider);

    menu->addSection(i18n("Icon Size"));
    menu->addAction(sliderAction);




    // configure the button and assign menu
    m_d->uiWdgPaintOpPresetSettings.presetChangeViewToolButton->setMenu(menu);
    m_d->uiWdgPaintOpPresetSettings.presetChangeViewToolButton->setIcon(KisIconUtils::loadIcon("view-choose"));
    m_d->uiWdgPaintOpPresetSettings.presetChangeViewToolButton->setPopupMode(QToolButton::InstantPopup);


    // show/hide buttons

    KisConfig cfg;

    m_d->uiWdgPaintOpPresetSettings.showScratchpadButton->setCheckable(true);
    m_d->uiWdgPaintOpPresetSettings.showScratchpadButton->setChecked(cfg.scratchpadVisible());
    m_d->uiWdgPaintOpPresetSettings.showEditorButton->setCheckable(true);
    m_d->uiWdgPaintOpPresetSettings.showEditorButton->setChecked(true);

    m_d->uiWdgPaintOpPresetSettings.showPresetsButton->setText(i18n("Presets"));
    m_d->uiWdgPaintOpPresetSettings.showPresetsButton->setCheckable(true);
    m_d->uiWdgPaintOpPresetSettings.showPresetsButton->setChecked(false);  // use a config to load/save this state
    slotSwitchShowPresets(false); // hide presets by default


    // Connections

    connect(m_d->uiWdgPaintOpPresetSettings.paintPresetIcon, SIGNAL(clicked()),
               m_d->uiWdgPaintOpPresetSettings.scratchPad, SLOT(paintPresetImage()));

    connect(saveDialog, SIGNAL(resourceSelected(KoResource*)), this, SLOT(resourceSelected(KoResource*)));

    connect (m_d->uiWdgPaintOpPresetSettings.renameBrushPresetButton, SIGNAL(clicked(bool)),
            this, SLOT(slotRenameBrushActivated()));

    connect (m_d->uiWdgPaintOpPresetSettings.cancelBrushNameUpdateButton, SIGNAL(clicked(bool)),
            this, SLOT(slotRenameBrushDeactivated()));

    connect(m_d->uiWdgPaintOpPresetSettings.updateBrushNameButton, SIGNAL(clicked(bool)),
            this, SLOT(slotSaveRenameCurrentBrush()));

    connect(m_d->uiWdgPaintOpPresetSettings.renameBrushNameTextField, SIGNAL(returnPressed()),
            SLOT(slotSaveRenameCurrentBrush()));

    connect(iconSizeSlider, SIGNAL(sliderMoved(int)),
            m_d->uiWdgPaintOpPresetSettings.presetWidget, SLOT(slotSetIconSize(int)));

    connect(iconSizeSlider, SIGNAL(sliderReleased()),
            m_d->uiWdgPaintOpPresetSettings.presetWidget, SLOT(slotSaveIconSize()));


    connect(m_d->uiWdgPaintOpPresetSettings.showScratchpadButton, SIGNAL(clicked(bool)),
            this, SLOT(slotSwitchScratchpad(bool)));

    connect(m_d->uiWdgPaintOpPresetSettings.showEditorButton, SIGNAL(clicked(bool)),
            this, SLOT(slotSwitchShowEditor(bool)));

    connect(m_d->uiWdgPaintOpPresetSettings.showPresetsButton, SIGNAL(clicked(bool)), this, SLOT(slotSwitchShowPresets(bool)));

    connect(m_d->uiWdgPaintOpPresetSettings.eraseScratchPad, SIGNAL(clicked()),
            m_d->uiWdgPaintOpPresetSettings.scratchPad, SLOT(fillDefault()));

    connect(m_d->uiWdgPaintOpPresetSettings.fillLayer, SIGNAL(clicked()),
            m_d->uiWdgPaintOpPresetSettings.scratchPad, SLOT(fillLayer()));

    connect(m_d->uiWdgPaintOpPresetSettings.fillGradient, SIGNAL(clicked()),
            m_d->uiWdgPaintOpPresetSettings.scratchPad, SLOT(fillGradient()));

    connect(m_d->uiWdgPaintOpPresetSettings.fillSolid, SIGNAL(clicked()),
            m_d->uiWdgPaintOpPresetSettings.scratchPad, SLOT(fillBackground()));


    m_d->settingsWidget = 0;
    setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));

    connect(m_d->uiWdgPaintOpPresetSettings.saveBrushPresetButton, SIGNAL(clicked()),
            this, SLOT(slotSaveBrushPreset()));

    connect(m_d->uiWdgPaintOpPresetSettings.saveNewBrushPresetButton, SIGNAL(clicked()),
            this, SLOT(slotSaveNewBrushPreset()));

    connect(m_d->uiWdgPaintOpPresetSettings.reloadPresetButton, SIGNAL(clicked()),
            this, SIGNAL(reloadPresetClicked()));

    connect(m_d->uiWdgPaintOpPresetSettings.bnDefaultPreset, SIGNAL(clicked()),
            this, SIGNAL(defaultPresetClicked()));

    connect(m_d->uiWdgPaintOpPresetSettings.dirtyPresetCheckBox, SIGNAL(toggled(bool)),
            this, SIGNAL(dirtyPresetToggled(bool)));

    connect(m_d->uiWdgPaintOpPresetSettings.eraserBrushSizeCheckBox, SIGNAL(toggled(bool)),
            this, SIGNAL(eraserBrushSizeToggled(bool)));

    connect(m_d->uiWdgPaintOpPresetSettings.eraserBrushOpacityCheckBox, SIGNAL(toggled(bool)),
            this, SIGNAL(eraserBrushOpacityToggled(bool)));


    // preset widget connections
    connect(m_d->uiWdgPaintOpPresetSettings.presetWidget->smallPresetChooser, SIGNAL(resourceSelected(KoResource*)),
            this, SIGNAL(signalResourceSelected(KoResource*)));

    connect(m_d->uiWdgPaintOpPresetSettings.reloadPresetButton, SIGNAL(clicked()),
            m_d->uiWdgPaintOpPresetSettings.presetWidget->smallPresetChooser, SLOT(updateViewSettings()));



    connect(m_d->uiWdgPaintOpPresetSettings.reloadPresetButton, SIGNAL(clicked()), SLOT(slotUpdatePresetSettings()));




    m_d->detached = false;
    m_d->ignoreHideEvents = false;
    m_d->minimumSettingsWidgetSize = QSize(0, 0);
    m_d->uiWdgPaintOpPresetSettings.scratchpadControls->setVisible(cfg.scratchpadVisible());
    m_d->detachedGeometry = QRect(100, 100, 0, 0);
    m_d->uiWdgPaintOpPresetSettings.dirtyPresetCheckBox->setChecked(cfg.useDirtyPresets());
    m_d->uiWdgPaintOpPresetSettings.eraserBrushSizeCheckBox->setChecked(cfg.useEraserBrushSize());
    m_d->uiWdgPaintOpPresetSettings.eraserBrushOpacityCheckBox->setChecked(cfg.useEraserBrushOpacity());

    m_d->uiWdgPaintOpPresetSettings.wdgLodAvailability->setCanvasResourceManager(resourceProvider->resourceManager());

    connect(resourceProvider->resourceManager(),
            SIGNAL(canvasResourceChanged(int,QVariant)),
            SLOT(slotResourceChanged(int, QVariant)));

    connect(m_d->uiWdgPaintOpPresetSettings.wdgLodAvailability,
            SIGNAL(sigUserChangedLodAvailability(bool)),
            SLOT(slotLodAvailabilityChanged(bool)));

    slotResourceChanged(KisCanvasResourceProvider::LodAvailability,
                        resourceProvider->resourceManager()->
                            resource(KisCanvasResourceProvider::LodAvailability));


    connect(m_d->uiWdgPaintOpPresetSettings.brushEgineComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdatePaintOpFilter()));

}


void KisPaintOpPresetsPopup::slotRenameBrushActivated()
{
   toggleBrushRenameUIActive(true);
}

void KisPaintOpPresetsPopup::slotRenameBrushDeactivated()
{
   toggleBrushRenameUIActive(false);
}

void KisPaintOpPresetsPopup::toggleBrushRenameUIActive(bool isRenaming)
{
    // This function doesn't really do anything except get the UI in a state to rename a brush preset

    m_d->uiWdgPaintOpPresetSettings.renameBrushPresetButton->setVisible(!isRenaming);
    m_d->uiWdgPaintOpPresetSettings.currentBrushNameLabel->setVisible(!isRenaming);
    m_d->uiWdgPaintOpPresetSettings.reloadPresetButton->setVisible(!isRenaming);

    m_d->uiWdgPaintOpPresetSettings.renameBrushNameTextField->setVisible(isRenaming);
    m_d->uiWdgPaintOpPresetSettings.updateBrushNameButton->setVisible(isRenaming);
    m_d->uiWdgPaintOpPresetSettings.cancelBrushNameUpdateButton->setVisible(isRenaming);

    m_d->uiWdgPaintOpPresetSettings.saveBrushPresetButton->setEnabled(!isRenaming);
    m_d->uiWdgPaintOpPresetSettings.saveNewBrushPresetButton->setEnabled(!isRenaming);
}

void KisPaintOpPresetsPopup::slotSaveRenameCurrentBrush()
{
    // if you are renaming a brush, that is different than updating the settings
    // make sure we are in a clean state before renaming. This logic might change,
    // but that is what we are going with for now
    emit reloadPresetClicked();


     m_d->favoriteResManager->setBlockUpdates(true);

    // get a reference to the existing (and new) file name and path that we are working with
    KisPaintOpPresetSP curPreset = m_d->resourceProvider->currentPreset();

    if (!curPreset)
        return;

    KisPaintOpPresetResourceServer * rServer = KisResourceServerProvider::instance()->paintOpPresetServer();
    QString saveLocation = rServer->saveLocation();

    QString originalPresetName = curPreset->name();
    QString renamedPresetName = m_d->uiWdgPaintOpPresetSettings.renameBrushNameTextField->text();
    QString originalPresetPathAndFile = saveLocation + originalPresetName + curPreset->defaultFileExtension();
    QString renamedPresetPathAndFile = saveLocation + renamedPresetName + curPreset->defaultFileExtension();


    // create a new brush preset with the name specified and add to resource provider
    KisPaintOpPresetSP newPreset = curPreset->clone();
    newPreset->setFilename(renamedPresetPathAndFile); // this also contains the path
    newPreset->setName(renamedPresetName);
    newPreset->setImage(curPreset->image()); // use existing thumbnail (might not need to do this)
    newPreset->setPresetDirty(false);
    newPreset->setValid(true);
    rServer->addResource(newPreset);

    resourceSelected(newPreset.data()); // refresh and select our freshly renamed resource


    // Now blacklist the original file
    if (rServer->resourceByName(originalPresetName)) {
         rServer->removeResourceAndBlacklist(curPreset);
     }

    m_d->favoriteResManager->setBlockUpdates(false);

    toggleBrushRenameUIActive(false); // this returns the UI to its original state after saving

    slotUpdatePresetSettings(); // update visibility of dirty preset and icon
}


void KisPaintOpPresetsPopup::slotResourceChanged(int key, const QVariant &value)
{
    if (key == KisCanvasResourceProvider::LodAvailability) {
        m_d->uiWdgPaintOpPresetSettings.wdgLodAvailability->slotUserChangedLodAvailability(value.toBool());
    }
}

void KisPaintOpPresetsPopup::slotLodAvailabilityChanged(bool value)
{
    m_d->resourceProvider->resourceManager()->setResource(KisCanvasResourceProvider::LodAvailability, QVariant(value));
}

KisPaintOpPresetsPopup::~KisPaintOpPresetsPopup()
{
    if (m_d->settingsWidget) {
        m_d->layout->removeWidget(m_d->settingsWidget);
        m_d->settingsWidget->hide();
        m_d->settingsWidget->setParent(0);
        m_d->settingsWidget = 0;
    }
    delete m_d;
}

void KisPaintOpPresetsPopup::setPaintOpSettingsWidget(QWidget * widget)
{
    if (m_d->settingsWidget) {
        m_d->layout->removeWidget(m_d->settingsWidget);
        m_d->uiWdgPaintOpPresetSettings.frmOptionWidgetContainer->updateGeometry();
    }
    m_d->layout->update();
    updateGeometry();

    m_d->widgetConnections.clear();
    m_d->settingsWidget = 0;

    if (widget) {

        m_d->settingsWidget = dynamic_cast<KisPaintOpConfigWidget*>(widget);
        KIS_ASSERT_RECOVER_RETURN(m_d->settingsWidget);

        if (m_d->settingsWidget->supportScratchBox()) {
            showScratchPad();
        } else {
            hideScratchPad();
        }

        m_d->widgetConnections.addConnection(m_d->settingsWidget, SIGNAL(sigConfigurationItemChanged()),
                                             this, SLOT(slotUpdateLodAvailability()));

        widget->setFont(m_d->smallFont);

        QSize hint = widget->sizeHint();
        m_d->minimumSettingsWidgetSize = QSize(qMax(hint.width(), m_d->minimumSettingsWidgetSize.width()),
                                               qMax(hint.height(), m_d->minimumSettingsWidgetSize.height()));
        widget->setMinimumSize(m_d->minimumSettingsWidgetSize);
        m_d->layout->addWidget(widget);

        // hook up connections that will monitor if our preset is dirty or not. Show a notification if it is
        if (m_d->resourceProvider && m_d->resourceProvider->currentPreset() ) {

            KisPaintOpPresetSP preset = m_d->resourceProvider->currentPreset();
            m_d->widgetConnections.addConnection(preset->updateProxy(), SIGNAL(sigSettingsChanged()),
                                      this, SLOT(slotUpdatePresetSettings()));

        }

        m_d->layout->update();
        widget->show();

    }

    slotUpdateLodAvailability();
}

void KisPaintOpPresetsPopup::slotUpdateLodAvailability()
{
    if (!m_d->settingsWidget) return;

    KisPaintopLodLimitations l = m_d->settingsWidget->lodLimitations();
    m_d->uiWdgPaintOpPresetSettings.wdgLodAvailability->setLimitations(l);
}


void KisPaintOpPresetsPopup::slotUpdatePresetSettings()
{
    if (!m_d->resourceProvider) {
        return;
    }
    if (!m_d->resourceProvider->currentPreset()) {
        return;
    }


    bool isPresetDirty = m_d->resourceProvider->currentPreset()->isPresetDirty();

    // don't need to reload or overwrite a clean preset
    m_d->uiWdgPaintOpPresetSettings.dirtyPresetIndicatorButton->setVisible(isPresetDirty);
    m_d->uiWdgPaintOpPresetSettings.reloadPresetButton->setVisible(isPresetDirty);
    m_d->uiWdgPaintOpPresetSettings.saveBrushPresetButton->setEnabled(isPresetDirty);



}


QImage KisPaintOpPresetsPopup::cutOutOverlay()
{
    return m_d->uiWdgPaintOpPresetSettings.scratchPad->cutoutOverlay();
}

void KisPaintOpPresetsPopup::contextMenuEvent(QContextMenuEvent *e)
{
    Q_UNUSED(e);
}

void KisPaintOpPresetsPopup::switchDetached(bool show)
{
    if (parentWidget()) {

        m_d->detached = !m_d->detached;

        if (m_d->detached) {
            m_d->ignoreHideEvents = true;

            if (show) {
                parentWidget()->show();
            }
            m_d->ignoreHideEvents = false;

        }
        else {
            KisConfig cfg;
            parentWidget()->hide();
        }

        KisConfig cfg;
        cfg.setPaintopPopupDetached(m_d->detached);
    }
}

void KisPaintOpPresetsPopup::hideScratchPad()
{
    m_d->uiWdgPaintOpPresetSettings.scratchPad->setEnabled(false);
    m_d->uiWdgPaintOpPresetSettings.fillGradient->setEnabled(false);
    m_d->uiWdgPaintOpPresetSettings.fillSolid->setEnabled(false);
    m_d->uiWdgPaintOpPresetSettings.eraseScratchPad->setEnabled(false);
}

void KisPaintOpPresetsPopup::showScratchPad()
{
    m_d->uiWdgPaintOpPresetSettings.scratchPad->setEnabled(true);
    m_d->uiWdgPaintOpPresetSettings.fillGradient->setEnabled(true);
    m_d->uiWdgPaintOpPresetSettings.fillSolid->setEnabled(true);
    m_d->uiWdgPaintOpPresetSettings.eraseScratchPad->setEnabled(true);
}

void KisPaintOpPresetsPopup::resourceSelected(KoResource* resource)
{
    // this gets called every time the brush editor window is opened
    // TODO: this gets called multiple times whenever the preset is changed in the presets area
    // the connections probably need to be thought about with this a bit more to keep things in sync

    m_d->uiWdgPaintOpPresetSettings.presetWidget->smallPresetChooser->setCurrentResource(resource);

    // find the display name of the brush engine and append it to the selected preset display
    QString currentBrushEngineName;
    for(int i=0; i < sortedBrushEnginesList.length(); i++) {
        if (sortedBrushEnginesList.at(i).id == currentPaintOpId() ) {
             currentBrushEngineName = sortedBrushEnginesList.at(i).name;
        }
    }

    QString selectedBrush = resource->name().append(" (").append(currentBrushEngineName).append(" ").append("Engine").append(")");

    m_d->uiWdgPaintOpPresetSettings.currentBrushNameLabel->setText(selectedBrush);
    m_d->uiWdgPaintOpPresetSettings.renameBrushNameTextField->setText(resource->name());


    // get the preset image and pop it into the thumbnail area on the top of the brush editor
    QGraphicsScene * thumbScene = new QGraphicsScene(this);
    thumbScene->addPixmap(QPixmap::fromImage(resource->image().scaled(30, 30)));
    thumbScene->setSceneRect(0, 0, 30, 30); // 30 x 30 image for thumb. this is also set in the UI
    m_d->uiWdgPaintOpPresetSettings.presetThumbnailicon->setScene(thumbScene);

    toggleBrushRenameUIActive(false); // reset the UI state of renaming a brush if we are changing brush presets
    slotUpdatePresetSettings(); // check to see if the dirty preset icon needs to be shown
}

bool variantLessThan(const KisPaintOpInfo v1, const KisPaintOpInfo v2)
{
    return v1.priority < v2.priority;
}

void KisPaintOpPresetsPopup::setPaintOpList(const QList< KisPaintOpFactory* >& list)
{
       m_d->uiWdgPaintOpPresetSettings.brushEgineComboBox->clear(); // reset combobox list just in case


       // create a new list so we can sort it and populate the brush engine combo box
       sortedBrushEnginesList.clear(); // just in case this function is called again, don't keep adding to the list

        for(int i=0; i < list.length(); i++) {

            QString fileName = KoResourcePaths::findResource("kis_images", list.at(i)->pixmap());
            QPixmap pixmap(fileName);

            if(pixmap.isNull()){
                pixmap = QPixmap(22,22);
                pixmap.fill();
            }

            KisPaintOpInfo paintOpInfo;
            paintOpInfo.id = list.at(i)->id();
            paintOpInfo.name = list.at(i)->name();
            paintOpInfo.icon = pixmap;
            paintOpInfo.priority = list.at(i)->priority();

            sortedBrushEnginesList.append(paintOpInfo);
        }

        std::stable_sort(sortedBrushEnginesList.begin(), sortedBrushEnginesList.end(), variantLessThan );

        // add an "All" option at the front to show all presets
        QPixmap emptyPixmap = QPixmap(22,22);
        emptyPixmap.fill(palette().color(QPalette::Background));
        sortedBrushEnginesList.push_front(KisPaintOpInfo(QString("all_options"), i18n("All"), QString(""), emptyPixmap, 0 ));

        // fill the list into the brush combo box
        for (int m = 0; m < sortedBrushEnginesList.length(); m++) {
            m_d->uiWdgPaintOpPresetSettings.brushEgineComboBox->addItem(sortedBrushEnginesList[m].icon, sortedBrushEnginesList[m].name, QVariant(sortedBrushEnginesList[m].id));
        }


}


void KisPaintOpPresetsPopup::setCurrentPaintOpId(const QString& paintOpId)
{
    current_paintOpId = paintOpId;
}


QString KisPaintOpPresetsPopup::currentPaintOpId() {
    return current_paintOpId;
}

void KisPaintOpPresetsPopup::setPresetImage(const QImage& image)
{
    m_d->uiWdgPaintOpPresetSettings.scratchPad->setPresetImage(image);
    saveDialog->brushPresetThumbnailWidget->setPresetImage(image);
}

void KisPaintOpPresetsPopup::hideEvent(QHideEvent *event)
{
    if (m_d->ignoreHideEvents) {
        return;
    }
    if (m_d->detached) {
        m_d->detachedGeometry = window()->geometry();
    }
    QWidget::hideEvent(event);
}

void KisPaintOpPresetsPopup::showEvent(QShowEvent *)
{
    if (m_d->detached) {
        window()->setGeometry(m_d->detachedGeometry);
    }
    emit brushEditorShown();
}

void KisPaintOpPresetsPopup::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    emit sizeChanged();
}

bool KisPaintOpPresetsPopup::detached() const
{
    return m_d->detached;
}

void KisPaintOpPresetsPopup::slotSwitchScratchpad(bool visible)
{
    m_d->uiWdgPaintOpPresetSettings.scratchpadControls->setVisible(visible);
    KisConfig cfg;
    cfg.setScratchpadVisible(visible);
    calculateShowingTopArea();
}

void KisPaintOpPresetsPopup::calculateShowingTopArea() {
    // if the scratchpad is the only area visible, we should hide the currently selected brush and settings
    // so the top area doesn't

    bool shouldDisplayTopBar = true;

    if (m_d->uiWdgPaintOpPresetSettings.showScratchpadButton->isChecked() && !m_d->uiWdgPaintOpPresetSettings.showPresetsButton->isChecked() &&
            !m_d->uiWdgPaintOpPresetSettings.showEditorButton->isChecked() ) {
        shouldDisplayTopBar = false;
    }

    m_d->uiWdgPaintOpPresetSettings.presetThumbnailicon->setVisible(shouldDisplayTopBar);
    m_d->uiWdgPaintOpPresetSettings.currentBrushNameLabel->setVisible(shouldDisplayTopBar);
    m_d->uiWdgPaintOpPresetSettings.renameBrushPresetButton->setVisible(shouldDisplayTopBar);

    // always hide these since they are part of the brush renaming field and can make things get in a weird state
    m_d->uiWdgPaintOpPresetSettings.renameBrushNameTextField->setVisible(false);
    m_d->uiWdgPaintOpPresetSettings.updateBrushNameButton->setVisible(false);
    m_d->uiWdgPaintOpPresetSettings.cancelBrushNameUpdateButton->setVisible(false);

    slotUpdatePresetSettings(); // find out if the preset is dirty or not and update visibility

}


void KisPaintOpPresetsPopup::slotSwitchShowEditor(bool visible) {
   m_d->uiWdgPaintOpPresetSettings.brushEditorSettingsControls->setVisible(visible);
   calculateShowingTopArea();
}

void KisPaintOpPresetsPopup::slotSwitchShowPresets(bool visible) {
    m_d->uiWdgPaintOpPresetSettings.presetsContainer->setVisible(visible);
    calculateShowingTopArea() ;
}

void KisPaintOpPresetsPopup::slotUpdatePaintOpFilter() {
    QVariant userData = m_d->uiWdgPaintOpPresetSettings.brushEgineComboBox->currentData(); // grab paintOpID from data
    QString filterPaintOpId = userData.toString();


    if (filterPaintOpId == "all_options") {
        filterPaintOpId = "";
    }
    m_d->uiWdgPaintOpPresetSettings.presetWidget->setPresetFilter(filterPaintOpId);
}

void KisPaintOpPresetsPopup::slotSaveBrushPreset() {
    // here we are assuming that people want to keep their existing preset icon. We will just update the
    // settings and save a new copy with the same name.
    // there is a dialog with save options, but we don't need to show it in this situation

    saveDialog->isSavingNewBrush(false); // this mostly just makes sure we keep the existing brush preset name when saving
    saveDialog->loadExistingThumbnail(); // This makes sure we use the existing preset icon when updating the existing brush preset
    saveDialog->savePreset();

    // refresh the view settings so the brush doesn't appear dirty
    slotUpdatePresetSettings();
}

void KisPaintOpPresetsPopup::slotSaveNewBrushPreset() {
    saveDialog->isSavingNewBrush(true);
    saveDialog->saveScratchPadThumbnailArea(m_d->uiWdgPaintOpPresetSettings.scratchPad->cutoutOverlay());
    saveDialog->showDialog();
}


void KisPaintOpPresetsPopup::updateViewSettings()
{
    m_d->uiWdgPaintOpPresetSettings.presetWidget->smallPresetChooser->updateViewSettings();
}

void KisPaintOpPresetsPopup::currentPresetChanged(KisPaintOpPresetSP preset)
{
     m_d->uiWdgPaintOpPresetSettings.presetWidget->smallPresetChooser->setCurrentResource(preset.data());
     setCurrentPaintOpId(preset->paintOp().id());
}

void KisPaintOpPresetsPopup::updateThemedIcons()
 {
    m_d->uiWdgPaintOpPresetSettings.fillLayer->setIcon(KisIconUtils::loadIcon("document-new"));
    m_d->uiWdgPaintOpPresetSettings.fillLayer->hide();
    m_d->uiWdgPaintOpPresetSettings.fillGradient->setIcon(KisIconUtils::loadIcon("krita_tool_gradient"));
    m_d->uiWdgPaintOpPresetSettings.fillSolid->setIcon(KisIconUtils::loadIcon("krita_tool_color_fill"));
    m_d->uiWdgPaintOpPresetSettings.eraseScratchPad->setIcon(KisIconUtils::loadIcon("edit-delete"));
    m_d->uiWdgPaintOpPresetSettings.presetChangeViewToolButton->setIcon(KisIconUtils::loadIcon("view-choose"));
}
