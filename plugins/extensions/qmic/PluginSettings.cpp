/*
 *  Copyright (c) 2017 Boudewijn Rempt <boud@kogmbh.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */
#include "PluginSettings.h"

#include <QVBoxLayout>
#include <QStandardPaths>

#include <kconfiggroup.h>
#include <klocalizedstring.h>

#include <KoIcon.h>

#include <kis_file_name_requester.h>
#include "kis_config.h"

PluginSettings::PluginSettings(QWidget *parent)
    : KisPreferenceSet(parent)
{
    setupUi(this);
    fileRequester->setFileName(gmicQtPath());
    fileRequester->setStartDir(QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
}

PluginSettings::~PluginSettings()
{
    KisConfig().writeEntry<QString>("gmic_qt_plugin_path", fileRequester->fileName());
}

QString PluginSettings::id()
{
    return QString("qmicsettings");
}

QString PluginSettings::name()
{
    return header();
}

QString PluginSettings::header()
{
    return QString(i18n("G'Mic-Qt Integration"));
}


QIcon PluginSettings::icon()
{
    return koIcon("gmic");
}

QString PluginSettings::gmicQtPath()
{
    QString gmicqt = "gmic_krita_qt";
#ifdef Q_OS_WIN
    gmicqt += ".exe";
#endif

    QString gmicqt_path = KisConfig().readEntry<QString>("gmic_qt_plugin_path",  qApp->applicationDirPath() + "/" + gmicqt);

    QFileInfo fi(gmicqt_path);
    if (!fi.exists() || !fi.isFile()) {

        QFileInfo fi2(qApp->applicationDirPath() + "/" + gmicqt);

        if (fi2.exists() && fi2.isFile()) {
            gmicqt_path = fi2.canonicalFilePath();
        }
        else {
            gmicqt_path.clear();
        }
    }

    return gmicqt;
}


void PluginSettings::savePreferences() const
{
    KisConfig().writeEntry<QString>("gmic_qt_plugin_path", fileRequester->fileName());
    Q_EMIT(settingsChanged());
}

void PluginSettings::loadPreferences()
{
    QString gmicqt("gmic_host_krita");
#ifdef Q_OS_WIN
    gmicqt += ".exe";
#endif
    fileRequester->setFileName(gmicQtPath());
}

void PluginSettings::loadDefaultPreferences()
{
    fileRequester->setFileName(gmicQtPath());
}
