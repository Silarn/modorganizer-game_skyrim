#include "skyrimgameplugins.h"
#include <safewritefile.h>
#include <report.h>
#include <ipluginlist.h>
#include <report.h>
#include <scopeguard.h>

#include <QDir>
#include <QTextCodec>
#include <QStringList>


using MOBase::IPluginGame;
using MOBase::IPluginList;
using MOBase::IOrganizer;
using MOBase::SafeWriteFile;
using MOBase::reportError;

SkyrimGamePlugins::SkyrimGamePlugins(IOrganizer *organizer)
    : GamebryoGamePlugins(organizer)
{
    m_LocalCodec = QTextCodec::codecForName("Windows-1252");
}

QStringList SkyrimGamePlugins::readPluginList(MOBase::IPluginList *pluginList)
{
    QStringList plugins = pluginList->pluginNames();
    QStringList primaryPlugins = organizer()->managedGame()->primaryPlugins();
    QStringList loadOrder(primaryPlugins);

    for (const QString &pluginName : loadOrder) {
        if (pluginList->state(pluginName) != IPluginList::STATE_MISSING) {
            pluginList->setState(pluginName, IPluginList::STATE_ACTIVE);
        }
    }

    // Do not sort the primary plugins. Their load order should be locked as defined in "primaryPlugins".
    for (QString plugin : plugins) {
        if (primaryPlugins.contains(plugin, Qt::CaseInsensitive))
            plugins.removeAll(plugin);
    }

    // Determine plugin active state by the plugins.txt file.
    bool pluginsTxtExists = true;
    QString filePath = organizer()->profile()->absolutePath() + "/plugins.txt";
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        pluginsTxtExists = false;
    }
    ON_BLOCK_EXIT([&]() {
        qDebug("close %s", qPrintable(filePath));
        file.close();
    });

    if (file.size() == 0) {
        // MO stores at least a header in the file. if it's completely empty the
        // file is broken
        pluginsTxtExists = false;
    }

    QStringList disabledPlugins;
    if (pluginsTxtExists) {
        while (!file.atEnd()) {
            QByteArray line = file.readLine();
            QString pluginName;
            if ((line.size() > 0) && (line.at(0) != '#')) {
                pluginName = m_LocalCodec->toUnicode(line.trimmed().constData());
            }
            if (pluginName.size() > 0) {
                pluginList->setState(pluginName, IPluginList::STATE_ACTIVE);
                plugins.removeAll(pluginName);
                disabledPlugins.append(pluginName);
                loadOrder.append(pluginName);
            }
        }

        file.close();

        // we removed each plugin found in the file, so what's left are inactive mods
        for (const QString &pluginName : plugins) {
            pluginList->setState(pluginName, IPluginList::STATE_INACTIVE);
        }
    } else {
        for (const QString &pluginName : plugins) {
            pluginList->setState(pluginName, IPluginList::STATE_INACTIVE);
        }
    }

    return loadOrder + disabledPlugins;
}
