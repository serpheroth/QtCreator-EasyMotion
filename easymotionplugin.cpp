#include "easymotionplugin.h"
#include "easymotionconstants.h"

#include <coreplugin/icore.h>
#include <coreplugin/icontext.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/actionmanager/command.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/coreconstants.h>
#include <texteditor/texteditor.h>
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
#include <QLabel>
#include <QStatusBar>
#include <QDebug>
#include <QPainter>
#include <QString>
#include <QTextBlock>
#include <QChar>
#include <QTextDocument>
#include <QTextBlock>
#include <QPair>
#include <algorithm>

using namespace EasyMotion::Internal;
using namespace EasyMotion;
using namespace Core;

namespace EasyMotion
{

template <class Editor>
QPair<int, int> getFirstAndLastVisiblePosition(Editor *editor)
{
  QTextCursor cursor = editor->textCursor();
  QTextDocument *doc = editor->document();
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
  return QPair<int, int>(firstPos, lastPos);
}

template <class Editor>
void moveToPosition(Editor *editor, int newPos, bool visualMode)
{
    QTextBlock targetBlock = editor->document()->findBlock(newPos);
    if (!targetBlock.isValid())
        targetBlock = editor->document()->lastBlock();

    bool overwriteMode = editor->overwriteMode();
    TextEditor::TextEditorWidget *baseEditor =
            qobject_cast<TextEditor::TextEditorWidget*>(editor);
    bool visualBlockMode = baseEditor && baseEditor->hasBlockSelection();

    bool selectNextCharacter = (overwriteMode || visualMode) && !visualBlockMode;
    bool keepSelection = visualMode || visualBlockMode;

    QTextCursor textCursor = editor->textCursor();
    textCursor.setPosition(selectNextCharacter ? newPos : newPos + 1,
                           keepSelection ? QTextCursor::KeepAnchor : QTextCursor::MoveAnchor);

    if (baseEditor)
        baseEditor->setTextCursor(textCursor);
    else
        editor->setTextCursor(textCursor);

    if (visualBlockMode) {
        baseEditor->setBlockSelection(false);
        baseEditor->setBlockSelection(true);
    }
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
  void searchTargetFromCurrentLine(QEditor *editor)
  {
    m_targetPos.clear();
    if (editor == NULL) {
      return;
    }
    m_currentGroup = 0;
    QTextDocument *doc = editor->document();
    int cursorPos = editor->textCursor().position();
    QTextBlock currentLineBlock = doc->findBlock(cursorPos);
    int firstPos = currentLineBlock.position();
    int lastPos = firstPos + currentLineBlock.length() - 1;
    for (int offset = 1; cursorPos - offset >= firstPos || cursorPos + offset <= lastPos; offset++) {
      if (cursorPos + offset <= lastPos) {
        m_targetPos << (cursorPos + offset);
      }
      if (cursorPos - offset >= firstPos) {
        m_targetPos << (cursorPos - offset);
      }
    }
  }

  template <class QEditor>
  void searchTargetFromScreen(QEditor *editor, const QChar &target)
  {
    m_targetPos.clear();
    if (editor == NULL) {
      return;
    }
    m_currentGroup = 0;
    m_targetChar = target;
    QTextDocument *doc = editor->document();
    int cursorPos = editor->textCursor().position();
    QPair<int, int> visibleRange = getFirstAndLastVisiblePosition(editor);
    int  firstPos = visibleRange.first;
    int lastPos = visibleRange.second;
    bool notCaseSensative = target.category() != QChar::Letter_Uppercase;
    for (int offset = 1; cursorPos - offset >= firstPos || cursorPos + offset <= lastPos; offset++) {
      if (cursorPos + offset <= lastPos) {
        QChar c = doc->characterAt(cursorPos + offset);
        if (notCaseSensative) {
          c = c.toLower();
        }
        if (c == target) {
          m_targetPos << (cursorPos + offset);
        }
      }
      if (cursorPos - offset >= firstPos) {
        QChar c = doc->characterAt(cursorPos - offset);
        if (notCaseSensative) {
          c = c.toLower();
        }
        if (c == target) {
          m_targetPos << (cursorPos - offset);
        }
      }
    }
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
    m_currentGroup++;
    if (m_currentGroup >= getGroupNum()) {
      m_currentGroup = 0;
    }
  }

  void previousGroup(void)
  {
    m_currentGroup--;
    if (m_currentGroup  < 0) {
      m_currentGroup = getGroupNum() - 1;
      if (m_currentGroup < 0) {
        m_currentGroup = 0;
      }
    }
  }

  void clear()
  {
    m_currentGroup = 0;
    m_targetPos.clear();
  }

  int getFirstTargetIndex(void) const
  {
    return m_currentGroup * GroupSize;
  }

  int getLastTargetIndex(void) const
  {
    int onePastLastIndex = m_currentGroup * GroupSize + 62;
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
      return QPair<int, QChar>(m_targetPos[i], m_code[i % GroupSize]);
    }
  }

  int getGroupNum(void)
  {
    if (m_targetPos.size() == 0) {
      return 0;
    } else {
      return (m_targetPos.size() - 1) / GroupSize + 1;
    }
  }

  QChar getTargetChar(void) const
  {
    return m_targetChar;
  }

  int getTargetPos(const QChar &c) const
  {
    int pos = parseCode(c);
    if (pos < 0) {
      return pos;
    } else {
      pos += m_currentGroup * GroupSize;
      if (pos < m_targetPos.size()) {
        return m_targetPos[pos];
      } else {
        return -1;
      }
    }
  }


private:
  int parseCode(const QChar &c) const
  {
    int index = -1;
    if (c.isLower()) {
      index = LowerLetterCaseStart + c.unicode() - ushort('a');
    } else if (c.isDigit()) {
      index = DigitStart + c.unicode() - ushort('0');
    } else if (c.isUpper()) {
      index = UpperLetterCaseStart + c.unicode() - ushort('A');
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
    LowerLetterCaseStart = 0,
    DigitStart = 26,
    UpperLetterCaseStart = 36,
    GroupSize = 62
  };
  int m_currentGroup;
  QChar m_targetChar;
  QVector<int> m_targetPos;
  QVector<QChar> m_code;
};

#define EDITOR(e) ((m_plainEdit != NULL) ? m_plainEdit->e : m_textEdit->e)

class EasyMotionHandler : public QObject
{
  Q_OBJECT

public:
  EasyMotionHandler(QObject *parent = 0)
    : QObject(parent)
    , m_currentEditor(NULL)
    , m_plainEdit(NULL)
    , m_textEdit(NULL)
    , m_fakeVimStatusWidget(0)
    , m_state(DefaultState)
    , m_easyMotionSearchRange(-1)
  {
      QMetaObject::invokeMethod(this, "findFakeVimStatusWidget", Qt::QueuedConnection);
  }

  ~EasyMotionHandler() {}

public slots:
  void easyMotionForCurrentLineTriggered(void)
  {
    m_easyMotionSearchRange = CurrentLine;
    initEasyMotion();
    QKeyEvent event(QEvent::None, Qt::Key_0, Qt::NoModifier);
    handleKeyPress(&event);
  }

  void easyMotionForEntireScreenTriggered(void)
  {
    m_easyMotionSearchRange = EntireScreen;
    initEasyMotion();
  }

private slots:
  void doInstallEventFilter()
  {
    if (m_plainEdit || m_textEdit) {
      EDITOR(installEventFilter(this));
      EDITOR(viewport())->installEventFilter(this);
    }
  }

  void findFakeVimStatusWidget()
  {
      QWidget *statusBar = Core::ICore::statusBar();
      foreach (QWidget *w, statusBar->findChildren<QWidget*>()) {
          if (QLatin1String(w->metaObject()->className()) == QLatin1String("FakeVim::Internal::MiniBuffer")) {
              m_fakeVimStatusWidget = w->findChild<QLabel*>();
              break;
          }
      }
  }

private:
  void installEventFilter()
  {
    // Postpone installEventFilter() so plugin gets next key event first.
    QMetaObject::invokeMethod(this, "doInstallEventFilter", Qt::QueuedConnection);
  }

  void initEasyMotion()
  {
    resetEasyMotion();
    m_currentEditor = EditorManager::currentEditor();
    if (setEditor(m_currentEditor)) {
      m_state = EasyMotionTriggered;
      installEventFilter();
    } else {
      m_currentEditor = NULL;
    }
  }

  void resetEasyMotion(void)
  {
    if (setEditor(m_currentEditor)) {
      QWidget *viewport = EDITOR(viewport());
      EDITOR(removeEventFilter(this));
      viewport->removeEventFilter(this);
      unsetEditor();
    }
    m_target.clear();
    m_state = DefaultState;
    m_currentEditor = NULL;
  }

  bool isVisualMode() const
  {
      if (m_fakeVimStatusWidget)
          return m_fakeVimStatusWidget->text().contains(QLatin1String("VISUAL"));
      return (m_plainEdit || m_textEdit) && EDITOR(textCursor()).hasSelection();
  }

  bool eventFilter(QObject *obj, QEvent *event)
  {
    QWidget *currentViewport = qobject_cast<QWidget *>(obj);
    if (currentViewport != NULL
        && event->type() == QEvent::Paint)  {
      // Handle the painter event last to prevent
      // the area painted by EasyMotion to be overidden
      currentViewport->removeEventFilter(this);
      QCoreApplication::sendEvent(currentViewport, event);
      currentViewport->installEventFilter(this);
      QPaintEvent *paintEvent = static_cast<QPaintEvent *>(event);
      handlePaintEvent(paintEvent);
      return true;
    } else if (event->type() == QEvent::KeyPress) {
      if (m_plainEdit || m_textEdit) {
        //        QMessageBox::information(Core::ICore::mainWindow(),
        //                                 tr("Action triggered"),
        //                                 tr("key"));
        installEventFilter();
        QKeyEvent *e = static_cast<QKeyEvent *>(event);
        bool keyPressHandled = handleKeyPress(e);
        return keyPressHandled;
      }
    }  else if (event->type() == QEvent::ShortcutOverride) {
      installEventFilter();
      // Handle ESC key press.
      QKeyEvent *e = static_cast<QKeyEvent *>(event);
      if (e->key() == Qt::Key_Escape)
        return handleKeyPress(e);
    }
    return false;
  }

  bool handleKeyPress(QKeyEvent *e)
  {
    if (e->key() == Qt::Key_Escape) {
      EasyMotionState tmpState = m_state;
      if (tmpState == WaitForInputTargetCode) {
        EDITOR(viewport()->update());
      }
      resetEasyMotion();
      return true;
    }  else if (m_state == EasyMotionTriggered && !isModifierKey(e->key())) {
      QChar target(e->key());
      target = target.toLower();
      if (e->modifiers() == Qt::ShiftModifier) target = target.toUpper();
      if (m_plainEdit) {
        if (m_easyMotionSearchRange == EntireScreen) {
          m_target.searchTargetFromScreen(m_plainEdit, target);
        } else {
          m_target.searchTargetFromCurrentLine(m_plainEdit);
        }
      } else if (m_textEdit) {
        if (m_easyMotionSearchRange == EntireScreen) {
          m_target.searchTargetFromScreen(m_textEdit, target);
        } else {
          m_target.searchTargetFromCurrentLine(m_textEdit);
        }
      } else {
        qDebug() << "EasyMotionHandler::handleKeyPress() => Error: current editor is null";
      }
      if (!m_target.isEmpty()) {
        m_state = WaitForInputTargetCode;
        EDITOR(viewport()->update());
      }
      return true;
    } else if (m_state == WaitForInputTargetCode && !isModifierKey(e->key())) {
      if (e->key() == Qt::Key_Return) {
        if (e->modifiers() == Qt::ShiftModifier) {
          // Shift + Enter makes EasyMotion show previous
          // group of target positions
          m_target.previousGroup();
        } else {
          // Enter makes EasyMotion show next
          // group of target positions
          m_target.nextGroup();
        }
        EDITOR(viewport()->update());
      } else {
        QChar target(e->key());
        target = target.toLower();
        if (e->modifiers() == Qt::ShiftModifier) target = target.toUpper();
        int newPos = m_target.getTargetPos(target);
        if (newPos >= 0) {
          QPlainTextEdit* plainEdit = m_plainEdit;
          QTextEdit* textEdit = m_textEdit;
          QWidget *viewport = EDITOR(viewport());
          resetEasyMotion();
          if (plainEdit)
              moveToPosition(plainEdit, newPos, isVisualMode());
          else if (textEdit)
              moveToPosition(textEdit, newPos, isVisualMode());
          viewport->update();
        }
      }
      return true;
    }
    return false;
  }

  bool handlePaintEvent(QPaintEvent *e)
  {
    Q_UNUSED(e);
    if (m_state == WaitForInputTargetCode && !m_target.isEmpty()) {
      QTextCursor tc = EDITOR(textCursor());
      QFontMetrics fm(EDITOR(font()));
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
        int targetCharFontWidth = fm.width(EDITOR(document())->characterAt(target.first));
        if (targetCharFontWidth == 0) targetCharFontWidth = fm.width(QChar(ushort(' ')));
        rect.setWidth(targetCharFontWidth);
        if (rect.intersects(EDITOR(viewport()->rect()))) {
          setTextPosition(rect);
          painter.setPen(Qt::NoPen);
          painter.drawRect(rect);
          painter.setPen(pen);
          int textHeight = rect.bottom() - fm.descent();
          painter.drawText(rect.left(), textHeight, QString(target.second));
        }
      }
      painter.end();
    }
    return false;
  }

  void setTextPosition(QRect &rect)
  {
    if (m_easyMotionSearchRange == CurrentLine) {
      int textHeightOffset = EDITOR(cursorRect()).height();
      rect.setTop(rect.top() - textHeightOffset);
      rect.setBottom(rect.bottom() - textHeightOffset);
      if (!rect.intersects(EDITOR(viewport()->rect()))) {
        rect.setTop(rect.top() + 2 * textHeightOffset);
        rect.setBottom(rect.bottom() + 2 * textHeightOffset);
      }
    }
  }

  bool isModifierKey(int key)
  {
    return key == Qt::Key_Control
           || key == Qt::Key_Shift
           || key == Qt::Key_Alt
           || key == Qt::Key_Meta;
  }

  bool setEditor(Core::IEditor *e)
  {
    if (e == NULL) return false;
    QWidget *widget = e->widget();
    m_plainEdit = qobject_cast<QPlainTextEdit *>(widget);
    m_textEdit = qobject_cast<QTextEdit *>(widget);
    return m_plainEdit != NULL || m_textEdit != NULL;
  }

  void unsetEditor()
  {
    m_plainEdit = NULL;
    m_textEdit = NULL;
  }

  enum EasyMotionState {
    DefaultState,
    EasyMotionTriggered,
    WaitForInputTargetCode
  };
  Core::IEditor *m_currentEditor;
  QPlainTextEdit *m_plainEdit;
  QTextEdit *m_textEdit;
  QLabel *m_fakeVimStatusWidget;
  EasyMotionState m_state;
  EasyMotion::EasyMotionTarget m_target;
  int m_easyMotionSearchRange;
  enum SearchRange {
    EntireScreen,
    CurrentLine
  };


};

} // namespace EasyMotion

EasyMotionPlugin::EasyMotionPlugin()
  : m_handler(new EasyMotionHandler)
{
  // Create your members
}

EasyMotionPlugin::~EasyMotionPlugin()
{
  // Unregister objects from the plugin manager's object pool
  // Delete members
  delete m_handler;
}

bool EasyMotionPlugin::initialize(const QStringList &arguments, QString *errorString)
{
  // Register objects in the plugin manager's object pool
  // Load settings
  // Add actions to menus
  // Connect to other plugins' signals
  // In the initialize function, a plugin can be sure that the plugins it
  // depends on have initialized their members.
  Q_UNUSED(arguments)
  Q_UNUSED(errorString)
  QAction *easyMotionSearchEntireScreen = new QAction(tr("Search entire screen"), this);
  Core::Command *searchScreenCmd = Core::ActionManager::registerAction(easyMotionSearchEntireScreen , Constants::SEARCH_SCREEN_ID,
                                   Core::Context(Core::Constants::C_GLOBAL));
  searchScreenCmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+;")));
  connect(easyMotionSearchEntireScreen , SIGNAL(triggered()), m_handler, SLOT(easyMotionForEntireScreenTriggered()));
  //------------------------------------------------------------------------------
  QAction *easyMotionSearchCurrentLine = new QAction(tr("Search current line"), this);
  Core::Command *searchCurrentLineCmd = Core::ActionManager::registerAction(easyMotionSearchCurrentLine , Constants::SEARCH_LINE_ID,
                                        Core::Context(Core::Constants::C_GLOBAL));
  searchCurrentLineCmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+'")));
  connect(easyMotionSearchCurrentLine , SIGNAL(triggered()), m_handler, SLOT(easyMotionForCurrentLineTriggered()));
  return true;
}

void EasyMotionPlugin::extensionsInitialized()
{
  // Retrieve objects from the plugin manager's object pool
  // In the extensionsInitialized function, a plugin can be sure that all
  // plugins that depend on it are completely initialized.
}

ExtensionSystem::IPlugin::ShutdownFlag EasyMotionPlugin::aboutToShutdown()
{
  // Save settings
  // Disconnect from signals that are not needed during shutdown
  // Hide UI (if you add UI that is not in the main window directly)
  return SynchronousShutdown;
}

#include "easymotionplugin.moc"
//Q_EXPORT_PLUGIN2(EasyMotion, EasyMotionPlugin)




