#ifndef EASYMOTION_H
#define EASYMOTION_H

#include "easymotion_global.h"

#include <extensionsystem/iplugin.h>

namespace EasyMotion {
class EasyMotionHandler;
namespace Internal {

class EasyMotionPlugin : public ExtensionSystem::IPlugin
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QtCreatorPlugin" FILE "EasyMotion.json")

public:
  EasyMotionPlugin();
  ~EasyMotionPlugin();

  bool initialize(const QStringList &arguments, QString *errorString);
  void extensionsInitialized();
  ShutdownFlag aboutToShutdown();

private slots:
private:
  EasyMotionHandler* m_handler;
};

} // namespace Internal
} // namespace EasyMotion

#endif // EASYMOTION_H

