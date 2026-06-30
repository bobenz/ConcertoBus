#include "LaunchSpec.h"

LaunchSpec::LaunchSpec(QObject *parent) : QObject(parent) {}

void LaunchSpec::setName(const QString &v)
{
    if (m_name == v) return;
    m_name = v;
    emit nameChanged();
}

void LaunchSpec::setTransport(const QString &v)
{
    if (m_transport == v) return;
    m_transport = v;
    emit transportChanged();
}

void LaunchSpec::setSubscribes(const QStringList &v)
{
    m_subscribes = v;
    emit subscribesChanged();
}

void LaunchSpec::setImportPaths(const QStringList &v)
{
    m_importPaths = v;
    emit importPathsChanged();
}

void LaunchSpec::setMainQml(const QString &v)
{
    if (m_mainQml == v) return;
    m_mainQml = v;
    emit mainQmlChanged();
}

void LaunchSpec::setAttachTo(const QString &v)
{
    if (m_attachTo == v) return;
    m_attachTo = v;
    emit attachToChanged();
}

