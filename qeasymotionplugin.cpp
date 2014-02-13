#include "qeasymotionplugin.h"
#include "qeasymotionconstants.h"

#include <coreplugin/icore.h>
#include <coreplugin/icontext.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/actionmanager/command.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/coreconstants.h>
#include <texteditor/basetexteditor.h>
#include <extensionsystem/pluginmanager.h>
#include <QListWidget>
#include <QAction>
#include <QMessageBox>
#include <QMainWindow>
#include <QMenu>

#include <QtPlugin>
#include <QObject>
#include <QApplication>
#include <QPlainTextEdit>
#include <QDebug>
#include <QPainter>
#include <QString>
#include <QTextBlock>
#include <QChar>
#include <QTextDocument>
#include <QTextBlock>
#include <QPair>
#include <algorithm>

using namespace QEasyMotion::Internal;
using namespace QEasyMotion;
using namespace Core;

namespace QEasyMotion
{
template <class Editor>
QPair<int, int> getFirstAndLastVisiblePosition(Editor* editor)
{
  QTextCursor cursor = editor->textCursor();
  QTextDocument* doc = editor->document();
  int currentLine = doc->findBlock(cursor.position()).blockNumber();
  int cursorHeight = editor->cursorRect().height();
  int lineCountToFirstVisibleLine = editor->cursorRect().top() / cursorHeight;
  int firstVisibleLineNum = currentLine - lineCountToFirstVisibleLine;
  if (firstVisibleLineNum < 0) {
    firstVisibleLineNum = 0;
  }
  int maxLineNumOnScreen = (editor->viewport()->height() / cursorHeight);
  if (maxLineNumOnScreen < 1) {
    maxLineNumOnScreen = 1;
  }
  int firstPos = doc->findBlockByNumber(firstVisibleLineNum).position();
  int lastVisibleLineNum = firstVisibleLineNum + maxLineNumOnScreen - 1;
  QTextBlock lastVisibleTextBlock = doc->findBlockByNumber(lastVisibleLineNum);
  if (!lastVisibleTextBlock.isValid()) {
    lastVisibleTextBlock = doc->lastBlock();
  }
  int lastPos = lastVisibleTextBlock.position() + lastVisibleTextBlock.length() - 1;
  //  qDebug() << firstPos << lastPos << firstVisibleLineNum << lastVisibleLineNum;
  return QPair<int, int>(firstPos, lastPos);
}

class EasyMotionTarget : public QObject
{
  Q_OBJECT
public:
  EasyMotionTarget(void)
  {
    initCode();
    m_targetPos.clear();
  }

  template <class QEditor>
  void setTarget(QEditor* editor, const QChar& target)
  {
    m_targetPos.clear();
    if (editor == NULL) {
      return;
    }
    m_currentGroup = 0;
    m_targetChar = target;
    QTextDocument* doc = editor->document();
    //debug
    //        QMessageBox::information(Core::ICore::mainWindow(),
    //                                 tr("Action triggered"),
    //        doc->toPlainText());
    QPair<int, int> visibleRange = getFirstAndLastVisiblePosition(editor);
    int firstPos = visibleRange.first;
    int lastVisiblePos = visibleRange.second;
    int cursorPos = editor->textCursor().position();
    for (int offset = 1; cursorPos - offset >= firstPos || cursorPos + offset <= lastVisiblePos; offset++) {
      if (cursorPos + offset <= lastVisiblePos) {
        QChar c = doc->characterAt(cursorPos + offset);
        if (c == target) {
          m_targetPos << (cursorPos + offset);
        }
      }
      if (cursorPos - offset >= firstPos) {
        QChar c = doc->characterAt(cursorPos - offset);
        if (c == target) {
          m_targetPos << (cursorPos - offset);
        }
      }
    }
    //#undef CALL_EDITOR
  }

  int size() const
  {
    return m_targetPos.size();
  }

  bool isEmpty() const
  {
    return m_targetPos.size() == 0;
  }

  void nextGroup(void)
  {
    if (m_currentGroup + 1 < getGroupNum()) {
      m_currentGroup++;
    }
  }

  void previousGroup(void)
  {
    if (m_currentGroup - 1 >= 0) {
      m_currentGroup--;
    }
  }

  void clear()
  {
    m_currentGroup = 0;
    m_targetPos.clear();
  }

  int getFirstTargetIndex(void) const
  {
    return m_currentGroup * kGroupSize;
  }

  int getLastTargetIndex(void) const
  {
    int onePastLastIndex = m_currentGroup * kGroupSize + 62;
    if (onePastLastIndex > m_targetPos.size()) {
      onePastLastIndex = m_targetPos.size();
    }
    return onePastLastIndex;
  }

  QPair<int, QChar> getTarget(int i) const
  {
    if (i < 0 || i > m_targetPos.size()) {
      return QPair<int, QChar>(int(-1), QChar(0));
    } else {
      return QPair<int, QChar>(m_targetPos[i], m_code[i % kGroupSize]);
    }
  }

  int getGroupNum(void)
  {
    if (m_targetPos.size() == 0) {
      return 0;
    } else {
      return (m_targetPos.size() - 1) / kGroupSize + 1;
    }
  }

  QChar getTargetChar(void) const
  {
    return m_targetChar;
  }

  int getTargetPos(const QChar& c) const
  {
    int pos = parseCode(c);
    if (pos < 0) {
      return pos;
    } else {
      pos += m_currentGroup * kGroupSize;
      if (pos < m_targetPos.size()) {
        return m_targetPos[pos];
      } else {
        return -1;
      }
    }
  }


private:
  int parseCode(const QChar& c) const
  {
    int index = -1;
    if (c.isLower()) {
      index = kLowerCaseStart + c.unicode() - ushort('a');
    } else if (c.isDigit()) {
      index = kDigitStart + c.unicode() - ushort('0');
    } else if (c.isUpper()) {
      index = kUpperCaseStart + c.unicode() - ushort('A');
    }
    return index;
  }

  void initCode(void)
  {
    m_code.reserve(62);
    for (int i = 0; i < 26; ++i) {
      m_code << QChar(i + 'a');
    }
    for (int i = 0; i < 10; ++i) {
      m_code << QChar(i + '0');
    }
    for (int i = 0; i < 26; ++i) {
      m_code << QChar(i + 'A');
    }
  }

  enum {
    kLowerCaseStart = 0,
    kDigitStart = 26,
    kUpperCaseStart = 36,
    kGroupSize = 62
  };
  int m_currentGroup;
  QChar m_targetChar;
  QVector<int> m_targetPos;
  QVector<QChar> m_code;
};

#define EDITOR(e) ((m_plainEdit != NULL) ? m_plainEdit->e : m_textEdit->e)

class QEasyMotionHandler : public QObject
{
  Q_OBJECT

public:
  QEasyMotionHandler(QObject* parent = 0)
    : QObject(parent)
    , m_plainEdit(NULL)
    , m_textEdit(NULL)
    , m_state(kDefault)
  {
  }

  ~QEasyMotionHandler() {}

  void initialize()
  {
    connect(EditorManager::instance(), SIGNAL(editorOpened(Core::IEditor*)),
            this, SLOT(editorOpened(Core::IEditor*)));
    connect(EditorManager::instance(), SIGNAL(editorAboutToClose(Core::IEditor*)),
            this, SLOT(editorAboutToClose(Core::IEditor*)));
  }

public slots:
  void editorAboutToClose(Core::IEditor* e)
  {
    setEditor(e);
    m_editors.remove(e);
    m_viewports.remove(EDITOR(viewport()));
    unsetEditor();
  }

  void editorOpened(Core::IEditor* e)
  {
    if (!setEditor(e)) return;
    if (m_editors.find(e) == m_editors.end()) {
      QWidget* viewport = EDITOR(viewport());
      m_editors.insert(e);
      m_viewports.insert(viewport, e);
      EDITOR(installEventFilter(this));
      viewport->installEventFilter(this);
    }
    unsetEditor();
  }

private:
  bool eventFilter(QObject* obj, QEvent* event)
  {
    m_currentViewport = qobject_cast<QWidget*>(obj);
    //    Core::IEditor* editor = qobject_cast<Core::IEditor*>(obj);
    //    bool validEasyMotionEvent = (viewport != NULL)
    //                                || (editor != NULL && m_editors.find(editor) != m_editors.end());
    //    if (!validEasyMotionEvent) return false;
    if (m_currentViewport != NULL
        && event->type() == QEvent::Paint)  {
      if (m_viewports.find(m_currentViewport) == m_viewports.end())  {
        QMessageBox::information(Core::ICore::mainWindow(),
                                 tr("Action triggered"),
                                 tr("note found"));
        return false;
      }
      setEditor(m_viewports.find(m_currentViewport).value());
      m_currentViewport->removeEventFilter(this);
      QCoreApplication::sendEvent(m_currentViewport, event);
      m_currentViewport->installEventFilter(this);
      handlePaintEvent((QPaintEvent*) event);
      m_currentViewport = NULL;
      unsetEditor();
      return true;
    } else if (event->type() == QEvent::KeyPress) {
      m_plainEdit = qobject_cast<QPlainTextEdit*>(obj);
      m_textEdit = qobject_cast<QTextEdit*>(obj);
      if (m_plainEdit || m_textEdit) {
        //        QMessageBox::information(Core::ICore::mainWindow(),
        //                                 tr("Action triggered"),
        //                                 tr("key"));
        QKeyEvent* e = (QKeyEvent*) event;
        bool keyPressHandled = handleKeyPress(e);
        unsetEditor();
        return keyPressHandled;
      } else {
        return false;
      }
    } else {
      return false;
    }
  }

  bool handleKeyPress(QKeyEvent * e)
  {
    if (e->key() == Qt::Key_Escape) {
      EasyMotionState tmpState = m_state;
      m_state = kDefault;
      if (tmpState == kWaitForInputTarget) {
        EDITOR(viewport()->update());
      }
      return true;
    } else if (e->modifiers() == Qt::ControlModifier && e->key() == Qt::Key_Semicolon) {
      m_target.clear();
      m_state = kStarted;
      return true;
    } else if (m_state == kStarted && !isModifierKey(e->key())) {
      QChar target(e->key());
      target = target.toLower();
      if (e->modifiers() == Qt::ShiftModifier) target = target.toUpper();
      if (m_plainEdit) {
        m_target.setTarget(m_plainEdit, target);
      } else {
        m_target.setTarget(m_textEdit, target);
      }
      if (!m_target.isEmpty()) {
        m_state = kWaitForInputTarget;
        EDITOR(viewport()->update());
        //        QMessageBox::information(Core::ICore::mainWindow(),
        //                                 tr("Action triggered"),
        //                                 tr("success"));
      }
      return true;
    } else if (m_state == kWaitForInputTarget && !isModifierKey(e->key())) {
      if (e->key() == Qt::Key_Return) {
        if (e->modifiers() == Qt::ShiftModifier) {
          m_target.previousGroup();
        } else {
          m_target.nextGroup();
        }
        EDITOR(viewport()->update());
      } else {
        QChar target(e->key());
        target = target.toLower();
        if (e->modifiers() == Qt::ShiftModifier) target = target.toUpper();
        int newPos = m_target.getTargetPos(target);
        if (newPos >= 0) {
          newPos++;
          QTextCursor cursor = EDITOR(textCursor());
          cursor.setPosition(newPos);
          EDITOR(setTextCursor(cursor));
          EDITOR(viewport()->update());
          m_state = kDefault;
        }
      }
      return true;
    }
    return false;
  }

  bool handlePaintEvent(QPaintEvent* e)
  {
    //    {
    //      // Draw text cursor.
    //      QRect rect = EDITOR(cursorRect());
    //      if (e->rect().intersects(rect)) {
    //        QPainter painter(EDITOR(viewport()));
    //        rect.setWidth(EDITOR(cursorWidth()));
    //        painter.setPen(EDITOR(palette().color(QPalette::Text)));
    //        painter.drawRect(rect);
    //      }
    //    }
    Q_UNUSED(e);
    if (m_state == kWaitForInputTarget && !m_target.isEmpty()) {
      QTextCursor tc = EDITOR(textCursor());
      QFontMetrics fm(EDITOR(font()));
      int targetCharFontWidth = fm.width(m_target.getTargetChar());
      QPainter painter(EDITOR(viewport()));
      QPen pen;
      pen.setColor(QColor(255, 0, 0, 255));
      painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
      painter.setBrush(QBrush(QColor(255, 255, 0, 255)));
      painter.setFont(EDITOR(font()));
      for (int i = m_target.getFirstTargetIndex(); i < m_target.getLastTargetIndex(); ++i) {
        QPair<int, QChar> target = m_target.getTarget(i);
        tc.setPosition(target.first);
        QRect rect = EDITOR(cursorRect(tc));
        rect.setWidth(targetCharFontWidth);
        if (rect.intersects(EDITOR(viewport()->rect()))) {
          painter.setPen(Qt::NoPen);
          painter.drawRect(rect);
          painter.setPen(pen);
          painter.drawText(rect.left(), rect.bottom() - fm.descent(), QString(target.second));
        }
      }
      painter.end();
    }
    return false;
  }

  bool isModifierKey(int key)
  {
    return key == Qt::Key_Control
           || key == Qt::Key_Shift
           || key == Qt::Key_Alt
           || key == Qt::Key_Meta;
  }

  bool setEditor(Core::IEditor* e)
  {
    if (e == NULL) return false;
    QWidget* widget = e->widget();
    m_plainEdit = qobject_cast<QPlainTextEdit*>(widget);
    m_textEdit = qobject_cast<QTextEdit*>(widget);
    return m_plainEdit != NULL || m_textEdit != NULL;
  }

  void unsetEditor()
  {
    m_plainEdit = NULL;
    m_textEdit = NULL;
  }

  enum EasyMotionState {
    kDefault,
    kStarted,
    kWaitForInputTarget
  };
  QSet<Core::IEditor*> m_editors;
  QHash<QWidget*, Core::IEditor*> m_viewports;
  //  QSet<QWidget*> m_viewports;
  QPlainTextEdit* m_plainEdit;
  QTextEdit* m_textEdit;
  QWidget* m_currentViewport;
  EasyMotionState m_state;
  QEasyMotion::EasyMotionTarget m_target;
};

} // namespace QEasyMotion
QEasyMotionPlugin::QEasyMotionPlugin()
  : m_handler(new QEasyMotionHandler)
{
  qDebug() << "here";
  // Create your members
}

QEasyMotionPlugin::~QEasyMotionPlugin()
{
  // Unregister objects from the plugin manager's object pool
  // Delete members
  delete m_handler;
}

bool QEasyMotionPlugin::initialize(const QStringList &arguments, QString *errorString)
{
  // Register objects in the plugin manager's object pool
  // Load settings
  // Add actions to menus
  // Connect to other plugins' signals
  // In the initialize function, a plugin can be sure that the plugins it
  // depends on have initialized their members.

  Q_UNUSED(arguments)
  Q_UNUSED(errorString)
  QAction *action = new QAction(tr("EasyMotion action"), this);
  Core::Command *cmd = Core::ActionManager::registerAction(action, Constants::ACTION_ID,
                                                           Core::Context(Core::Constants::C_GLOBAL));
  cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+;")));
  connect(action, SIGNAL(triggered()), this, SLOT(triggerAction()));

  Core::ActionContainer *menu = Core::ActionManager::createMenu(Constants::MENU_ID);
  menu->menu()->setTitle(tr("EasyMotion"));
  menu->addAction(cmd);
  Core::ActionManager::actionContainer(Core::Constants::M_TOOLS)->addMenu(menu);

  m_handler->initialize();
  return true;
}

void QEasyMotionPlugin::extensionsInitialized()
{
  // Retrieve objects from the plugin manager's object pool
  // In the extensionsInitialized function, a plugin can be sure that all
  // plugins that depend on it are completely initialized.
}

ExtensionSystem::IPlugin::ShutdownFlag QEasyMotionPlugin::aboutToShutdown()
{
  // Save settings
  // Disconnect from signals that are not needed during shutdown
  // Hide UI (if you add UI that is not in the main window directly)
  return SynchronousShutdown;
}

void QEasyMotionPlugin::triggerAction()
{
  ExtensionSystem::PluginManager* pm
    = ExtensionSystem::PluginManager::instance();

  QList<QObject*> objects = pm->allObjects();
  QListWidget* listWidget = new QListWidget;
  QString result;
  Q_FOREACH(QObject * obj, objects) {
    QString objInfo;
    const char* name = obj->metaObject()->className();
    QString sep(tr("===="));
    objInfo.append(obj->objectName()).append(sep).append(tr(name));
    result.append(objInfo).append(tr("\n"));
//    listWidget->addItem(objInfo);
  }
  QPlainTextEdit* edit = new QPlainTextEdit;
  edit->setPlainText(result);
  edit->show();
//  listWidget->show();
  return;
  QMessageBox::information(Core::ICore::mainWindow(),
                           tr("Action triggered"),
                           tr("This is an action from EasyMotion."));
}

#include "qeasymotionplugin.moc"
Q_EXPORT_PLUGIN2(QEasyMotion, QEasyMotionPlugin)

