#include "plugin.h"
#include "kpluginfactory.h"

#include "Krita.h"
#include "KisPart.h"
#include "KisViewManager.h"
#include "KisDocument.h"
#include "kis_node_manager.h"
#include "kis_node.h"
#include "kundo2stack.h"

#include <memory>
#include <string>

K_PLUGIN_FACTORY_WITH_JSON(KritaJLSHackPluginFactory, "kritajlshack.json",
                           registerPlugin<KritaJLSHackPlugin>();)

KritaJLSHackPlugin::KritaJLSHackPlugin(QObject *parent, const QVariantList &)
    : KisViewPlugin(parent)
	{
    qInfo() << "Loading JLS Hack plugin";
	auto k = Krita::instance();

	auto action = k->createAction("Autoname layers children");
	connect(action, SIGNAL(triggered(bool)),
	        this, SLOT(AutonameLayerChildren()));

	action = k->createAction("Export selected layer children");
	connect(action, SIGNAL(triggered(bool)),
	        this, SLOT(ExportLayerChildren()));
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
				n.children[i]->setName(n.old_names[i].c_str());

		if ( ! was_modified )
			doc->setModified(false);
		}

	void redo() override
		{
		for ( auto n : nodes )
			for ( auto i = 0u; i < n.children.size(); ++i )
				n.children[i]->setName(n.new_names[i].c_str());

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
	// @todo: iterate over selected layers
	qInfo() << "Export layer children";
	}

#include "plugin.moc"
