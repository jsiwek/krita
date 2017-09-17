#include "plugin.h"

#include "kpluginfactory.h"
#include "kundo2stack.h"

#include "kis_node_manager.h"
#include "kis_node.h"
#include "kis_mask.h"
#include "kis_effect_mask.h"
#include "kis_paint_layer.h"
#include "kis_painter.h"
#include "kis_image.h"
#include "kis_group_layer.h"

#include "KisPart.h"
#include "KisViewManager.h"
#include "KisDocument.h"
#include "KisMimeDatabase.h"

#include "KoProgressBar.h"

#include "Krita.h"

#include "qfileinfo.h"
#include "qjsondocument.h"
#include "qjsonobject.h"
#include "qjsonarray.h"

#include <string>
#include <memory>

K_PLUGIN_FACTORY_WITH_JSON(KritaJLSHackPluginFactory, "kritajlshack.json",
                           registerPlugin<KritaJLSHackPlugin>();)

KritaJLSHackPlugin::KritaJLSHackPlugin(QObject *parent, const QVariantList &)
    : KisViewPlugin(parent)
	{
	qInfo() << "Loading JLS Hack plugin";
	auto k = Krita::instance();

	{
	auto name = "Autoname children";
	auto action = k->createAction(name, name);
	connect(action, SIGNAL(triggered(bool)),
	        this, SLOT(AutonameLayerChildren()));
	}

	{
	auto name = "Export children";
	auto action = k->createAction(name, name);
	connect(action, SIGNAL(triggered(bool)),
	        this, SLOT(ExportLayerChildren()));
	}

	{
	auto name = "Quick export layers(s)";
	auto action = k->createAction(name, name);
	connect(action, SIGNAL(triggered(bool)),
	        this, SLOT(QuickExportLayer()));
	}
	}

static KisDocument* current_document()
	{
	auto window = KisPart::instance()->currentMainwindow();

	if ( ! window )
		return {};

	auto viewManager = window->viewManager();

	if ( ! viewManager )
		return {};

	return viewManager->document();
	}

static KisNodeList selected_nodes()
	{
	auto window = KisPart::instance()->currentMainwindow();

	if ( ! window )
		return {};

	auto viewManager = window->viewManager();

	if ( ! viewManager )
		return {};

	auto nodeManager = viewManager->nodeManager();

	if ( ! nodeManager )
		return {};

	return nodeManager->selectedNodes();
	}

struct AutonameUndoCommand : public KUndo2Command {

	AutonameUndoCommand(const std::string& text, KisDocument* doc,
	                    KisNodeList nodes_to_autoname)
	    : KUndo2Command(kundo2_noi18n(text.c_str())),
	      doc(doc),
	      nodes(),
	      was_modified(doc->isModified())
		{
		for ( auto n : nodes_to_autoname )
			{
			qInfo() << "  naming children of: " << n->name();
			AutonameNode an;
			an.node = n;

			for ( auto i = 0u; i < n->childCount(); ++i )
				{
				auto child = n->at(i);
				an.children.emplace_back(child);
				an.old_names.emplace_back(child->name().toStdString());

				QString new_name = n->name() + " ";

				if ( i < 10 )
					new_name += "00";
				else if ( i < 100 )
					new_name += "0";

				QString num;
				num.setNum(i);
				new_name += num;
				an.new_names.emplace_back(new_name.toStdString());

				if ( dynamic_cast<KisMask*>(child.data() ) )
					{
					qInfo() << "    " << child->name() << " -> " << "<skipped>";
					continue;
					}

				qInfo() << "    " << child->name() << " -> " << new_name;
				}

			nodes.emplace_back(an);
			}

		redo();
		}

	void undo() override
		{
		for ( auto n : nodes )
			for ( auto i = 0u; i < n.children.size(); ++i )
				{
				auto child = n.children[i];

				if ( dynamic_cast<KisMask*>(child.data() ) )
					continue;

				child->setName(n.old_names[i].c_str());
				}

		if ( ! was_modified )
			doc->setModified(false);
		}

	void redo() override
		{
		for ( auto n : nodes )
			for ( auto i = 0u; i < n.children.size(); ++i )
				{
				auto child = n.children[i];

				if ( dynamic_cast<KisMask*>(child.data() ) )
					continue;

				child->setName(n.new_names[i].c_str());
				}

		doc->setModified(true);
		}

	struct AutonameNode {
		KisNodeSP node;
		std::vector<KisNodeSP> children;
		std::vector<std::string> old_names;
		std::vector<std::string> new_names;
	};

	KisDocument* doc;
	std::vector<AutonameNode> nodes;
	bool was_modified;
};

void KritaJLSHackPlugin::AutonameLayerChildren()
	{
	qInfo() << "Autonaming layers";

	auto doc = current_document();

	if ( ! doc )
		return;

	auto nodes = selected_nodes();

	if ( nodes.empty() )
		{
		qInfo() << "  no nodes selected";
		return;
		}

	std::string text{"autoname:"};

	for ( auto n : nodes )
		text += " " + n->name().toStdString();

	doc->addCommand(new AutonameUndoCommand(text, doc, nodes));
	}

void KritaJLSHackPlugin::ExportLayerChildren()
	{
	qInfo() << "Export layer children";

	auto doc = current_document();
	auto file_info = QFileInfo(doc->url().toLocalFile());
	QString mime = "image/png";
	QString file_ext = KisMimeDatabase::suffixesForMimeType(mime).first();
	QString file_path = file_info.absolutePath();
	QString file_basename = file_info.baseName();
	KisImageSP image = doc->image();
	QRect r = image->bounds();

	auto nodes = selected_nodes();
	auto num_docs = 0;
	auto doc_num = 0;

	for ( auto n : nodes )
		for ( auto i = 0u; i < n->childCount(); ++i )
			if ( n->at(i)->visible() )
				++num_docs;

	KoProgressBar progress;
	progress.setRange(0, num_docs);

	for ( auto n : nodes )
		{
		qInfo() << "  export children of layer: " << n->name();
		QJsonArray parts;

		auto layer = dynamic_cast<KisLayer*>(n.data());

		if ( ! layer )
			{
			qInfo() << "  skipped, not a layer: " << n->name();
			continue;
			}

		auto effects = layer->effectMasks();

		for ( auto e : effects )
			qInfo() << "    found effect:" << e->name();

		for ( auto i = 0u; i < n->childCount(); ++i )
			{
			auto child = n->at(i);

			if ( ! child->visible() )
				{
				qInfo() << "    skip invisible layer: " << child->name();
				continue;
				}

			if ( dynamic_cast<KisMask*>(child.data() ) )
				{
				qInfo() << "    skip mask layer: " << child->name();
				continue;
				}

			qInfo() << "    export layer: " << child->name();

			std::unique_ptr<KisDocument> export_doc{
				KisPart::instance()->createDocument()};

			KisImageSP dst = new KisImage(export_doc->createUndoStore(),
			                              r.width(), r.height(),
			                              image->colorSpace(), child->name());
			dst->setResolution(image->xRes(), image->yRes());

			export_doc->setCurrentImage(dst);
			export_doc->setMimeType(mime.toLatin1());
			export_doc->setFileBatchMode(true);

			KisGroupLayer* groupLayer = new KisGroupLayer(dst, "group",
			                                              n->opacity());
			KisPaintLayer* paintLayer = new KisPaintLayer(dst,
			                                              "paint",
			                                              child->opacity());
			KisPainter gc(paintLayer->paintDevice());
			gc.bitBlt(QPoint(0, 0), child->projection(), r);
			dst->addNode(groupLayer, dst->rootLayer(), KisLayerSP(0));
			dst->addNode(paintLayer, groupLayer, KisLayerSP(0));
			dst->refreshGraph();

			QRect cropRect = paintLayer->projection()->nonDefaultPixelArea();

			for ( auto e : effects )
				e->apply(paintLayer->paintDevice(), cropRect, cropRect,
				         KisNode::N_FILTHY);

			dst->refreshGraph();

			if ( ! cropRect.isEmpty() )
				export_doc->image()->cropImage(cropRect);

			qInfo() << "      crop rect: " << cropRect;

			QString path = file_path + "/" + file_basename + "_" +
			               child->name().replace(' ', '_') + '.' + file_ext;
			QUrl url = QUrl::fromLocalFile(path);
			export_doc->exportDocument(url, mime.toLatin1());

			QJsonObject part;
			part["file"] = url.fileName();
			part["name"] = child->name();
			part["offsetX"] = cropRect.topLeft().x();
			part["offsetY"] = cropRect.topLeft().y();
			part["sizeX"] = cropRect.width();
			part["sizeY"] = cropRect.height();
			part["order"] = child->parent()->index(child);
			parts.push_back(part);
			progress.setValue(++doc_num);
			}

		QJsonObject json;
		json["name"] = n->name();
		json["origX"] = r.width();
		json["origY"] = r.height();
		json["order"] = n->parent()->index(n);
		json["parts"] = parts;
		QJsonDocument json_doc{json};

		QString json_path = file_path + "/" + file_basename + "_" +
		                    n->name().replace(' ', '_') + ".json";
		QFile json_file;
		json_file.setFileName(json_path);
		json_file.open(QFile::WriteOnly);
		json_file.write(json_doc.toJson());
		}
	}

void KritaJLSHackPlugin::QuickExportLayer()
	{
	qInfo() << "Quick export layers";

	auto doc = current_document();
	auto file_info = QFileInfo(doc->url().toLocalFile());
	QString mime = "image/png";
	QString file_ext = KisMimeDatabase::suffixesForMimeType(mime).first();
	QString file_path = file_info.absolutePath();
	QString file_basename = file_info.baseName();
	KisImageSP image = doc->image();
	QRect r = image->bounds();

	auto nodes = selected_nodes();
	KoProgressBar progress;
	progress.setRange(0, nodes.size());
	auto doc_num = 0;

	for ( auto n : nodes )
		{
		qInfo() << "    export layer: " << n->name();

		std::unique_ptr<KisDocument> export_doc{
			KisPart::instance()->createDocument()};

		KisImageSP dst = new KisImage(export_doc->createUndoStore(),
		                  r.width(), r.height(),
		                  image->colorSpace(), n->name());
		dst->setResolution(image->xRes(), image->yRes());

		export_doc->setCurrentImage(dst);
		export_doc->setMimeType(mime.toLatin1());
		export_doc->setFileBatchMode(true);

		KisPaintLayer* paintLayer = new KisPaintLayer(dst,
		                          "projection",
		                          n->opacity());
		KisPainter gc(paintLayer->paintDevice());
		gc.bitBlt(QPoint(0, 0), n->projection(), r);
		dst->addNode(paintLayer, dst->rootLayer(), KisLayerSP(0));
		dst->refreshGraph();

		QString path = file_path + "/" + file_basename + "_" +
		           n->name().replace(' ', '_') + '.' + file_ext;
		QUrl url = QUrl::fromLocalFile(path);
		export_doc->exportDocument(url, mime.toLatin1());
		progress.setValue(++doc_num);
		}
	}

#include "plugin.moc"
