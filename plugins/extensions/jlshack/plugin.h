#ifndef JLSHACK_PLUGIN_H_
#define JLSHACK_PLUGIN_H_

#include <QObject>

#include <kis_view_plugin.h>

class KritaJLSHackPlugin : public KisViewPlugin {
	Q_OBJECT

public:

    KritaJLSHackPlugin(QObject *parent, const QVariantList &);
	virtual ~KritaJLSHackPlugin() {}

private Q_SLOTS:

	void AutonameLayerChildren();
	void ExportLayerChildren();
	void QuickExportLayer();
};

#endif
