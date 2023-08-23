/**
   @file loader.cpp
   @brief Plugin loader

   <p>
   Copyright (C) 2009-2010 Nokia Corporation

   @author Timo Rongas <ext-timo.2.rongas@nokia.com>
   @author Semi Malinen <semi.malinen@nokia.com
   @author Ustun Ergenoglu <ext-ustun.ergenoglu@nokia.com>
   @author Antti Virtanen <antti.i.virtanen@nokia.com>

   This file is part of Sensord.

   Sensord is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License
   version 2.1 as published by the Free Software Foundation.

   Sensord is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with Sensord.  If not, see <http://www.gnu.org/licenses/>.
   </p>
 */

#include "loader.h"
#include "plugin.h"
#include <QPluginLoader>
#include <QStringList>
#include <QList>
#include <QDir>
#include <QCoreApplication>

#include "logging.h"
#include "config.h"

#ifdef USE_SSUSYSINFO
# include <ssusysinfo/ssusysinfo.h>
#endif

Loader::Loader()
{
    scanAvailablePlugins();
}

Loader& Loader::instance()
{
    static Loader the_loader;

    return the_loader;
}

#define PLUGIN_PREFIX_ENV "SENSORFW_LIBRARY_PATH"
#define PLUGIN_PREFIX     "lib"
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#define PLUGIN_SUFFIX     "-qt6.so"
#else
#define PLUGIN_SUFFIX     "-qt5.so"
#endif
#define SENSOR_SUFFIX     "sensor"

static QString getPluginDirectory()
{
    QByteArray env = qgetenv(PLUGIN_PREFIX_ENV);
    return QString::fromUtf8(env + PLUGIN_DIRECTORY);
}

static QString getPluginPath(const QString &name)
{
    return QString("%1/" PLUGIN_PREFIX "%2" PLUGIN_SUFFIX).arg(getPluginDirectory()).arg(name);
}

bool Loader::loadPluginFile(const QString &name, QString &errorString, QStringList &stack)
{
    const QString resolvedName(resolveRealPluginName(name));
    QPluginLoader qpl(getPluginPath(resolvedName));
    qpl.setLoadHints(QLibrary::ExportExternalSymbolsHint);
    QObject *object = 0;
    PluginBase *plugin = 0;
    sensordLogD() << "Loader loading plugin:" << resolvedName << "as:" << name << "from:" << qpl.fileName();
    bool loaded = false;
    bool cyclic = stack.contains(resolvedName);
    stack.prepend(resolvedName);
    if (cyclic) {
        errorString = "cyclic plugin dependency";
        sensordLogC() << "Plugin has cyclic dependency:" << resolvedName;
    } else if (loadedPluginNames_.contains(resolvedName)) {
        sensordLogD() << "Plugin is already loaded:" << resolvedName;
        loaded = true;
    } else if (!pluginAvailable(resolvedName)) {
        errorString = "plugin not available";
        sensordLogW() << "Plugin not available:" << resolvedName;
    } else if (!qpl.load()) {
        errorString = qpl.errorString();
        sensordLogC() << "Plugin loading error:" << resolvedName << "-" << errorString;
    } else if (!(object = qpl.instance())) {
        errorString = "not able to instanciate";
        sensordLogC() << "Plugin loading error: " << resolvedName << "-" << errorString;
    } else if (!(plugin = qobject_cast<PluginBase*>(object))) {
        errorString = "not a Plugin type";
        sensordLogC() << "Plugin loading error: " << resolvedName << "-" << errorString;
    } else {
        loaded = true;
        QStringList dependencies(plugin->Dependencies());
        sensordLogD() << resolvedName << "requires:" << dependencies;
        foreach (const QString &dependency, dependencies) {
            if (!(loaded = loadPluginFile(dependency, errorString, stack))) {
                break;
            }
        }
        if (loaded) {
            plugin->Register(*this);
            loadedPluginNames_.append(resolvedName);
            plugin->Init(*this);
        }
    }
    stack.removeOne(resolvedName);
    if (!loaded) {
        invalidatePlugin(resolvedName);
    }
    return loaded;
}

bool Loader::loadPlugin(const QString& name, QString *errorString)
{
    QString error;
    QStringList stack;
    bool loaded = loadPluginFile(name, error, stack);
    if (!loaded && errorString) {
        *errorString = error;
    }
    return loaded;
}

#ifdef USE_SSUSYSINFO
static ssusysinfo_t *ssusysinfo = 0;
#endif

static bool evaluateAvailabilityValue(const QString &name, const QString &val)
{
    bool available = true;
    if (val.startsWith("Feature_")) {
#ifdef USE_SSUSYSINFO
        const QStringList features(val.split("|"));
        bool allow = false;
        bool deny  = false;
        foreach(const QString &feature, features) {
            hw_feature_t id = ssusysinfo_hw_feature_from_name(feature.toUtf8().constData());
            if (id == Feature_Invalid ) {
                sensordLogW() << "unknown hw feature:" << feature;
                continue;
            }
            if( ssusysinfo_has_hw_feature(ssusysinfo, id) ) {
                allow = true;
                break;
            }
            deny = true;
        }
        if( deny && !allow ) {
            sensordLogD() << "plugin disabled in hw-config: " << name << "value" << val;
            available = false;
        }
#else
        // When compiled without ssu-support, these are enabled by design
        sensordLogD() << "sensor plugin enabled implicitly: " << name << "value" << val;
#endif
    } else if (val == "False") {
        sensordLogD() << "plugin disabled sensorfwd config: " << name << "value" << val;
        available = false;
    } else if (name.endsWith(SENSOR_SUFFIX) && val != "True") {
        // Warn about implicitly enabled sensor plugins
        sensordLogW() << "sensor plugin enabled implicitly: " << name << "value" << val;
    }
    return available;
}

void Loader::scanAvailablePlugins()
{
#ifdef USE_SSUSYSINFO
    if (!ssusysinfo) {
        ssusysinfo = ssusysinfo_create();
    }
#endif
    QStringList res;
    QDir dir(getPluginDirectory());
    dir.setFilter(QDir::Files | QDir::NoSymLinks | QDir::NoDot | QDir::NoDotDot);
    const QString prefix(PLUGIN_PREFIX);
    const QString suffix(PLUGIN_SUFFIX);
    foreach (QString file, dir.entryList()) {
        if (file.startsWith(prefix) && file.endsWith(suffix)) {
            int beg = prefix.size();
            int end = file.size() - suffix.size();
            const QString name(file.mid(beg, end-beg));
            QString key = QString("available/%1").arg(name);
            QString val = SensorFrameworkConfig::configuration()->value(key).toString();
            if( evaluateAvailabilityValue(name, val) ) {
                res.append(name);
            }
        }
    }
    availablePluginNames_ = res;
#ifdef USE_SSUSYSINFO
    ssusysinfo_delete(ssusysinfo), ssusysinfo = 0;
#endif
}

QStringList Loader::availablePlugins() const
{
    return availablePluginNames_;
}

QStringList Loader::availableSensorPlugins() const
{
    QStringList res;
    foreach(const QString &name, availablePluginNames_) {
        if (name.endsWith(SENSOR_SUFFIX)) {
            res.append(name);
        }
    }
    return res;
}

bool Loader::pluginAvailable(const QString &name) const
{
    return availablePluginNames_.contains(name);
}

void Loader::invalidatePlugin(const QString &name)
{
    if (availablePluginNames_.removeAll(name) > 0) {
        sensordLogW() << "plugin marked invalid: " << name;
    }
}

QString Loader::resolveRealPluginName(const QString& pluginName) const
{
    QString key = QString("plugins/%1").arg(pluginName);
    QString nameFromConfig = SensorFrameworkConfig::configuration()->value(key).toString();
    if (nameFromConfig.isEmpty()) {
        sensordLogT() << "Plugin setting for " << pluginName << " not found from configuration. Using key as value.";
        return pluginName;
    }
    return nameFromConfig;
}
