#include "BusClientQml.h"
#include <QQmlExtensionPlugin>

class ConcertoBusPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)
public:
    void registerTypes(const char *uri) override
    {
        Q_UNUSED(uri)
        // Qt6 qt_add_qml_module handles registration via QML_ELEMENT
    }
};

#include "ConcertoBusPlugin.moc"
