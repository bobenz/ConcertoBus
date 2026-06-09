#include "BusClientQml.h"

#include <QJsonArray>
#include <QMetaMethod>
#include <QMetaProperty>
#include <QDebug>

// ── SignalRelayBase ───────────────────────────────────────────────────────────
// A minimal Q_OBJECT class whose single slot "relay()" exists purely to give
// us a valid slot index for QMetaObject::connect.  It is NEVER actually called
// through the normal virtual dispatch path — SignalRelay (below) overrides
// qt_metacall and intercepts invocations of method-id 0, reading the raw
// argument array before it would be discarded by the zero-arg signature.

class SignalRelayBase : public QObject
{
    Q_OBJECT
public:
    explicit SignalRelayBase(QObject *parent = nullptr) : QObject(parent) {}

public slots:
    // NEVER call directly; invocation is intercepted in SignalRelay::qt_metacall.
    void relay() {}
};

// ── SignalRelay ───────────────────────────────────────────────────────────────

class SignalRelay : public SignalRelayBase
{
public:
    QMetaMethod signalMethod;
    std::function<void(QVariantList)> callback;

    explicit SignalRelay(QObject *parent = nullptr) : SignalRelayBase(parent) {}

    // Override qt_metacall to intercept relay() (method id 0 after QObject base
    // methods are handled) and extract raw signal arguments from void **a.
    int qt_metacall(QMetaObject::Call c, int id, void **a) override
    {
        // Let QObject handle its own methods (deleteLater etc.) and adjust id.
        id = QObject::qt_metacall(c, id, a);
        if (id < 0) return id;
        if (c == QMetaObject::InvokeMetaMethod && id == 0 && callback) {
            QVariantList args;
            const int n = signalMethod.parameterCount();
            for (int i = 0; i < n; ++i)
                args << QVariant(signalMethod.parameterMetaType(i), a[i + 1]);
            callback(args);
            return -1;
        }
        return id;
    }
};

// relay slot index in the SignalRelayBase meta-object (cached once at startup)
static int relaySlotIndex()
{
    static int idx = SignalRelayBase::staticMetaObject.indexOfSlot("relay()");
    return idx;
}

// ── BusForeign ────────────────────────────────────────────────────────────────

BusForeign::BusForeign(QObject *parent)
    : QObject(parent)
    , m_client(new BusClient(this))
{
    connect(m_client, &BusClient::messageReceived, this, &BusForeign::updateFromNetwork);
    connect(m_client, &BusClient::connectedChanged, this, [this]() {
        if (m_client->isConnected() && !m_tag.isEmpty())
            m_client->subscribe(topic());
    });
}

QString BusForeign::busHost() const { return m_client->host(); }
void BusForeign::setBusHost(const QString &h) { m_client->setHost(h); emit busHostChanged(); if (m_complete) reconnect(); }
int BusForeign::busPort() const { return m_client->port(); }
void BusForeign::setBusPort(int p) { m_client->setPort(p); emit busPortChanged(); if (m_complete) reconnect(); }

void BusForeign::setName(const QString &n)
{
    if (m_name == n) return;
    if (!m_tag.isEmpty() && m_client->isConnected())
        m_client->unsubscribe(topic());
    m_name = n;
    emit nameChanged();
    m_client->setName(n);
    if (m_complete) reconnect();
}

void BusForeign::setTag(const QString &t)
{
    if (m_tag == t) return;
    if (!m_tag.isEmpty() && m_client->isConnected())
        m_client->unsubscribe(topic());
    m_tag = t;
    emit tagChanged();
    if (m_complete) reconnect();
}

void BusForeign::componentComplete()
{
    m_complete = true;
    reconnect();
}

void BusForeign::reconnect()
{
    if (m_name.isEmpty() || m_tag.isEmpty()) return;
    m_client->connectToBus();
    if (m_client->isConnected())
        m_client->subscribe(topic());
}

QString BusForeign::topic() const
{
    return m_name + QLatin1Char('/') + m_tag;
}

void BusForeign::updateFromNetwork(const QString &, const QJsonObject &data)
{
    const QMetaObject *mo = metaObject();

    if (data.contains("properties")) {
        const QJsonObject props = data["properties"].toObject();
        for (auto it = props.begin(); it != props.end(); ++it) {
            const int idx = mo->indexOfProperty(it.key().toUtf8().constData());
            if (idx < 0) continue;
            const QMetaProperty mp = mo->property(idx);
            QVariant v = it.value().toVariant();
            if (v.convert(mp.metaType()))
                mp.write(this, v);
        }
    }

    if (data.contains("action")) {
        const QByteArray sig = data["action"].toString().toUtf8();
        const QJsonArray argsJson = data["args"].toArray();

        for (int i = mo->methodOffset(); i < mo->methodCount(); ++i) {
            QMetaMethod m = mo->method(i);
            if (m.methodType() != QMetaMethod::Signal) continue;
            if (m.name() != sig) continue;
            if (m.parameterCount() != argsJson.size()) continue;

            QList<QVariant> args;
            bool ok = true;
            for (int j = 0; j < m.parameterCount(); ++j) {
                QVariant v = argsJson[j].toVariant();
                if (!v.convert(m.parameterMetaType(j))) { ok = false; break; }
                args.append(v);
            }
            if (!ok) continue;

            QGenericArgument ga[9];
            for (int j = 0; j < args.size() && j < 9; ++j)
                ga[j] = QGenericArgument(m.parameterTypes().at(j).constData(), args[j].constData());

            m.invoke(this, Qt::QueuedConnection,
                     ga[0], ga[1], ga[2], ga[3], ga[4], ga[5], ga[6], ga[7], ga[8]);
            break;
        }
    }
}

// ── BusProxy ──────────────────────────────────────────────────────────────────

BusProxy::BusProxy(QObject *parent)
    : QObject(parent)
    , m_client(new BusClient(this))
{
    connect(m_client, &BusClient::connectedChanged, this, [this]() {
        if (m_client->isConnected() && m_complete && !m_hooksEstablished) {
            m_hooksEstablished = true;
            hookPropertiesAndSignals();
        }
    });
}

BusProxy::~BusProxy()
{
    clearHooks();
}

QString BusProxy::busHost() const { return m_client->host(); }
void BusProxy::setBusHost(const QString &h) { m_client->setHost(h); emit busHostChanged(); }
int BusProxy::busPort() const { return m_client->port(); }
void BusProxy::setBusPort(int p) { m_client->setPort(p); emit busPortChanged(); }

void BusProxy::setTargetProcess(const QString &t)
{
    if (m_target == t) return;
    m_target = t;
    emit targetProcessChanged();
    if (m_complete) reconnect();
}

void BusProxy::setTag(const QString &t)
{
    if (m_tag == t) return;
    m_tag = t;
    emit tagChanged();
    if (m_complete) reconnect();
}

void BusProxy::componentComplete()
{
    m_complete = true;
    reconnect();
}

void BusProxy::reconnect()
{
    if (m_target.isEmpty() || m_tag.isEmpty()) return;
    clearHooks();
    m_hooksEstablished = false;
    m_client->connectToBus();
    if (m_client->isConnected()) {
        m_hooksEstablished = true;
        hookPropertiesAndSignals();
    }
}

QString BusProxy::topic() const
{
    return m_target + QLatin1Char('/') + m_tag;
}

void BusProxy::hookPropertiesAndSignals()
{
    const QMetaObject *mo = metaObject();
    const int slotIdx = relaySlotIndex();

    for (int i = mo->propertyOffset(); i < mo->propertyCount(); ++i) {
        const QMetaProperty mp = mo->property(i);
        if (!mp.hasNotifySignal()) continue;
        const QString propName = QString::fromLatin1(mp.name());

        sendPropertyUpdate(propName, mp.read(this));

        auto *relay = new SignalRelay(this);
        relay->signalMethod = mp.notifySignal();
        relay->callback = [this, propName, mp](const QVariantList &) {
            sendPropertyUpdate(propName, mp.read(this));
        };
        QMetaObject::connect(this, mp.notifySignalIndex(), relay, slotIdx);
    }

    for (int i = mo->methodOffset(); i < mo->methodCount(); ++i) {
        const QMetaMethod method = mo->method(i);
        if (method.methodType() != QMetaMethod::Signal) continue;
        const QString name = QString::fromLatin1(method.name());
        if (name.endsWith("Changed")) continue;

        const int sigIdx = mo->indexOfMethod(method.methodSignature().constData());
        auto *relay = new SignalRelay(this);
        relay->signalMethod = method;
        relay->callback = [this, name](const QVariantList &args) {
            sendActionTrigger(name, args);
        };
        QMetaObject::connect(this, sigIdx, relay, slotIdx);
    }
}

void BusProxy::clearHooks()
{
    const auto relays = findChildren<SignalRelay *>(QString(), Qt::FindDirectChildrenOnly);
    for (auto *r : relays) delete r;
}

void BusProxy::sendPropertyUpdate(const QString &name, const QVariant &value)
{
    m_client->publish(topic(), QJsonObject{{"properties", QJsonObject{{name, QJsonValue::fromVariant(value)}}}});
}

void BusProxy::sendActionTrigger(const QString &name, const QVariantList &args)
{
    QJsonObject data{{"action", name}};
    if (!args.isEmpty()) {
        QJsonArray jarr;
        for (const QVariant &v : args)
            jarr << QJsonValue::fromVariant(v);
        data["args"] = jarr;
    }
    m_client->publish(topic(), data);
}

// automoc processes Q_OBJECT classes defined in .cpp files when this is included
#include "BusClientQml.moc"
