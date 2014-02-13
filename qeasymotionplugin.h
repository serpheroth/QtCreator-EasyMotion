#ifndef QEASYMOTION_H
#define QEASYMOTION_H

#include "qeasymotion_global.h"

#include <extensionsystem/iplugin.h>

namespace QEasyMotion {
class QEasyMotionHandler;
namespace Internal {

class QEasyMotionPlugin : public ExtensionSystem::IPlugin
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QtCreatorPlugin" FILE "QEasyMotion.json")

public:
  QEasyMotionPlugin();
  ~QEasyMotionPlugin();

  bool initialize(const QStringList &arguments, QString *errorString);
  void extensionsInitialized();
  ShutdownFlag aboutToShutdown();

private slots:
  void triggerAction();
private:
  QEasyMotionHandler* m_handler;
};

} // namespace Internal
} // namespace QEasyMotion

#endif // QEASYMOTION_H

