// BusClient and StdioBusClient register themselves via QML_ELEMENT.
// This plugin shell is required by the qt_add_qml_module machinery.
#include "BusClient.h"
#include "StdioBusClient.h"
#include <QQmlExtensionPlugin>

class ConcertoBusPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)
public:
    void registerTypes(const char *) override {}
};

#include "ConcertoBusPlugin.moc"
