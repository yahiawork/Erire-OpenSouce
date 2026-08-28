const state = {
  project: null,
  docs: [],
  activePath: "",
  treeEntries: [],
  collapsed: new Set(),
  explorerSelectedPath: "",
  explorerRenamePath: "",
  bottomTab: "terminal",
  output: [],
  terminal: [],
  terminalCwd: "",
  terminalInputHistory: [],
  terminalHistoryIndex: -1,
  terminalDraft: "",
  problems: [],
  process: "idle",
  settings: {
    erireExe: "erire.exe",
    pythonExe: "python.exe",
    fontName: "Cascadia Code",
    themeName: "dark-2026",
    fontSize: 14,
    autosave: false,
    softwareRendering: true,
    showExplorer: true,
    showOutline: true,
    showBottom: true,
  },
  ai: {
    endpoint: "https://erire.pythonanywhere.com/api/erire-ai-ide",
    productKey: "",
    keyPreview: "",
    includeFile: true,
    includeTree: true,
    includeProblems: true,
    history: [],
    loading: false,
  },
  autosaveTimer: null,
};

const editorWork = {
  paintQueued: false,
  bottomQueued: false,
  outlineTimer: 0,
};

const dialogState = {
  kind: "",
  resolver: null,
  returnFocus: null,
};

const completionState = {
  visible: false,
  items: [],
  selected: 0,
  prefix: "",
  start: 0,
  end: 0,
};

let openMenuName = "";

const MAX_HIGHLIGHT_LENGTH = 120000;
const MAX_OUTLINE_LINES = 4000;
const EDITOR_HISTORY_LIMIT = 160;
const AI_STORAGE_KEY = "erireStudio.aiSettings.v1";
const AI_HISTORY_LIMIT = 12;
const AI_MAX_ACTIVE_FILE_CHARS = 26000;

const $ = (id) => document.getElementById(id);
const ui = {
  projectTitle: $("project-title"),
  projectPath: $("project-path"),
  explorer: $("explorer-tree"),
  tabs: $("tab-strip"),
  welcome: $("welcome"),
  editorShell: $("editor-shell"),
  gutter: $("editor-gutter"),
  scroller: $("editor-scroller"),
  currentLine: $("editor-current-line"),
  highlight: $("editor-highlight"),
  input: $("editor-input"),
  outline: $("outline-list"),
  output: $("panel-output"),
  terminal: $("panel-terminal"),
  terminalHistory: $("terminal-history"),
  terminalPrompt: $("terminal-prompt"),
  terminalInput: $("terminal-input"),
  problems: $("panel-problems"),
  statusFile: $("status-file"),
  statusProject: $("status-project"),
  statusCursor: $("status-cursor"),
  statusLanguage: $("status-language"),
  statusProcess: $("status-process"),
  statusSave: $("status-save"),
  backdrop: $("modal-backdrop"),
  newProject: $("modal-new-project"),
  settings: $("modal-settings"),
  about: $("modal-about"),
  dialog: $("modal-dialog"),
  newProjectName: $("new-project-name"),
  newProjectLocation: $("new-project-location"),
  newProjectTemplate: $("new-project-template"),
  templateHint: $("template-hint"),
  settingsErire: $("settings-erire"),
  settingsPython: $("settings-python"),
  settingsFontName: $("settings-font-name"),
  settingsTheme: $("settings-theme"),
  settingsFontSize: $("settings-font-size"),
  settingsAutosave: $("settings-autosave"),
  dialogTitle: $("dialog-title"),
  dialogMessage: $("dialog-message"),
  dialogInputRow: $("dialog-input-row"),
  dialogInputLabel: $("dialog-input-label"),
  dialogInput: $("dialog-input"),
  dialogCancel: $("dialog-cancel"),
  dialogConfirm: $("dialog-confirm"),
  explorerPanel: document.querySelector('[data-panel="explorer"]'),
  outlinePanel: document.querySelector('[data-panel="outline"]'),
  bottomPanel: document.querySelector('[data-panel="bottom"]'),
  contextMenu: $("editor-context-menu"),
  completionPanel: $("completion-panel"),
  toastHost: $("toast-host"),
  aiStatus: $("ai-status"),
  aiEndpoint: $("ai-endpoint"),
  aiProductKey: $("ai-product-key"),
  aiIncludeFile: $("ai-include-file"),
  aiIncludeTree: $("ai-include-tree"),
  aiIncludeProblems: $("ai-include-problems"),
  aiMessages: $("ai-messages"),
  aiForm: $("ai-form"),
  aiInput: $("ai-input"),
  aiSend: $("ai-send"),
};

const bridge = {
  post(type, payload = {}) {
    const message = JSON.stringify({ type, payload });
    if (window.chrome?.webview) window.chrome.webview.postMessage(message);
    else console.info("bridge out", message);
  },
};

const templateHints = {
  default: "A balanced Erire desktop app with UI structure, Python bridge support, and starter content.",
  blank: "A clean blank window template for starting from zero with the smallest useful scaffold.",
  workspace: "A tool-style project with sidebar, content area, and status-driven layout for productivity apps.",
  "python-tool": "A desktop starter focused on Python bridge workflows, command actions, and data coming back into the UI.",
  showcase: "A richer presentation-style starter with polished sections for demos, prototypes, and visual showcases.",
};

const modalViews = {
  newProject: () => ui.newProject,
  settings: () => ui.settings,
  about: () => ui.about,
  dialog: () => ui.dialog,
};

const ERIRE_COMPLETIONS = [
  { label: "screen.create", insert: "screen.create[app; size; 100; 100; 900; 560]", detail: "Create window" },
  { label: "screen.title", insert: 'screen.title["|"]', detail: "Window title" },
  { label: "screen.bg", insert: 'screen.bg["#11161d"]', detail: "Window background" },
  { label: "screen.add", insert: "screen.add[|]", detail: "Add UI node" },
  { label: "screen.show", insert: 'screen.show["|"]', detail: "Show page" },
  { label: "screen.setText", insert: 'screen.setText["|"; ""]', detail: "Update node text" },
  { label: "var.set", insert: 'var.set["|"; ""]', detail: "Set variable" },
  { label: "import python", insert: 'import python "./python/app.py" as app', detail: "Python bridge" },
  { label: "py.call", insert: 'py.call["app.|"]', detail: "Call Python" },
  { label: "text.value", insert: 'text.value["|"]', detail: "Text node" },
  { label: "button.text", insert: 'button.text["|"].onClick[\n    \n]', detail: "Button handler" },
  { label: "input.placeholder", insert: 'input.placeholder["|"].bind["value"]', detail: "Bound input" },
  { label: "card.text", insert: 'card.text["|"]', detail: "Card node" },
  { label: "panel.text", insert: 'panel.text["|"]', detail: "Panel node" },
  { label: "image.src", insert: 'image.src["./assets/|"]', detail: "Image node" },
  { label: "webview.url", insert: 'webview.url["https://|"]', detail: "WebView node" },
  { label: "onLoad", insert: "onLoad[\n    |\n]", detail: "Startup event" },
  { label: "onClick", insert: "onClick[\n    |\n]", detail: "Click event" },
  { label: "if", insert: "if[|][\n    \n]", detail: "Condition" },
  { label: "else.if", insert: "else.if[|][\n    \n]", detail: "Else-if block" },
  { label: "else", insert: "else[\n    |\n]", detail: "Else block" },
  { label: "while", insert: "while[|][\n    \n]", detail: "Loop" },
  { label: "for", insert: 'for["i"; 1; 10][\n    |\n]', detail: "Range loop" },
  { label: "id", insert: 'id["|"]', detail: "Node id" },
  { label: "page", insert: 'page["|"]', detail: "Node page" },
  { label: "x", insert: "x[|]", detail: "Position x" },
  { label: "y", insert: "y[|]", detail: "Position y" },
  { label: "w", insert: "w[|]", detail: "Width" },
  { label: "h", insert: "h[|]", detail: "Height" },
  { label: "bg", insert: 'bg["#|"]', detail: "Background" },
  { label: "color", insert: 'color["#|"]', detail: "Text color" },
  { label: "size", insert: "size[|]", detail: "Text size" },
];

const PYTHON_COMPLETIONS = [
  { label: "def", insert: "def |():\n    pass", detail: "Function" },
  { label: "class", insert: "class |:\n    pass", detail: "Class" },
  { label: "if", insert: "if |:\n    pass", detail: "Condition" },
  { label: "elif", insert: "elif |:\n    pass", detail: "Else-if" },
  { label: "else", insert: "else:\n    |", detail: "Else" },
  { label: "for", insert: "for | in range():\n    pass", detail: "Loop" },
  { label: "while", insert: "while |:\n    pass", detail: "Loop" },
  { label: "try", insert: "try:\n    |\nexcept Exception as exc:\n    print(exc)", detail: "Try/except" },
  { label: "with", insert: "with | as value:\n    pass", detail: "Context" },
  { label: "import", insert: "import |", detail: "Import module" },
  { label: "from", insert: "from | import ", detail: "Import from module" },
  { label: "return", insert: "return |", detail: "Return value" },
  { label: "print", insert: "print(|)", detail: "Print" },
  { label: "True", insert: "True", detail: "Boolean" },
  { label: "False", insert: "False", detail: "Boolean" },
  { label: "None", insert: "None", detail: "None" },
];

const JSON_COMPLETIONS = [
  { label: "true", insert: "true", detail: "Boolean" },
  { label: "false", insert: "false", detail: "Boolean" },
  { label: "null", insert: "null", detail: "Null" },
  { label: "object", insert: "{\n  |\n}", detail: "Object" },
  { label: "array", insert: "[\n  |\n]", detail: "Array" },
];

const themePresets = {
  "dark-2026": {
    "--window-bg": "#11161d",
    "--panel-bg": "#161c24",
    "--panel-elevated": "#1b2330",
    "--border": "#283241",
    "--text": "#e6edf3",
    "--muted": "#94a3b8",
    "--subtle": "#64748b",
    "--accent": "#4f8cff",
    "--accent-hover": "#79a8ff",
    "--editor-bg": "#0f172a",
    "--editor-text": "#e2e8f0",
    "--log-bg": "#0b1220",
    "--status-bg": "#0b1016",
    "--toolbar-grad-start": "#1b2330",
    "--toolbar-grad-end": "#151c25",
    "--tab-bg": "#19222f",
    "--tab-active-bg": "#1d2a3d",
    "--tab-active-border": "#36507b",
    "--row-hover": "rgba(79, 140, 255, 0.08)",
    "--gutter-bg": "#0c1320",
    "--current-line-bg": "rgba(79, 140, 255, 0.08)",
    "--token-keyword": "#c792ea",
    "--token-string": "#98c379",
    "--token-number": "#f6c177",
    "--token-comment": "#63718a",
    "--token-punctuation": "#9fb0c8",
    "--token-decorator": "#ff9e64",
    "--token-function": "#82aaff",
    "--token-type": "#7dd3fc",
  },
  "vscode-dark": {
    "--window-bg": "#1e1e1e",
    "--panel-bg": "#252526",
    "--panel-elevated": "#2d2d30",
    "--border": "#3c3c3c",
    "--text": "#d4d4d4",
    "--muted": "#9da1a6",
    "--subtle": "#6e7681",
    "--accent": "#0e639c",
    "--accent-hover": "#1177bb",
    "--editor-bg": "#1e1e1e",
    "--editor-text": "#d4d4d4",
    "--log-bg": "#181818",
    "--status-bg": "#007acc",
    "--toolbar-grad-start": "#2d2d30",
    "--toolbar-grad-end": "#252526",
    "--tab-bg": "#2d2d30",
    "--tab-active-bg": "#1e1e1e",
    "--tab-active-border": "#007acc",
    "--row-hover": "rgba(14, 99, 156, 0.12)",
    "--gutter-bg": "#1e1e1e",
    "--current-line-bg": "rgba(255, 255, 255, 0.05)",
    "--token-keyword": "#c586c0",
    "--token-string": "#ce9178",
    "--token-number": "#b5cea8",
    "--token-comment": "#6a9955",
    "--token-punctuation": "#d4d4d4",
    "--token-decorator": "#dcdcaa",
    "--token-function": "#dcdcaa",
    "--token-type": "#4ec9b0",
  },
  abyss: {
    "--window-bg": "#0c1220",
    "--panel-bg": "#10192b",
    "--panel-elevated": "#142138",
    "--border": "#20314e",
    "--text": "#dce7ff",
    "--muted": "#8ea1c9",
    "--subtle": "#60739c",
    "--accent": "#5ea1ff",
    "--accent-hover": "#7fb4ff",
    "--editor-bg": "#08111f",
    "--editor-text": "#dbe9ff",
    "--log-bg": "#09101c",
    "--status-bg": "#08101b",
    "--toolbar-grad-start": "#16233d",
    "--toolbar-grad-end": "#0f182a",
    "--tab-bg": "#14223a",
    "--tab-active-bg": "#193055",
    "--tab-active-border": "#3564b0",
    "--row-hover": "rgba(94, 161, 255, 0.1)",
    "--gutter-bg": "#091221",
    "--current-line-bg": "rgba(94, 161, 255, 0.08)",
    "--token-keyword": "#ff79c6",
    "--token-string": "#a7f3d0",
    "--token-number": "#f9c74f",
    "--token-comment": "#6b7fa8",
    "--token-punctuation": "#9eb5d9",
    "--token-decorator": "#ffb86c",
    "--token-function": "#8be9fd",
    "--token-type": "#7dd3fc",
  },
  monokai: {
    "--window-bg": "#171717",
    "--panel-bg": "#222222",
    "--panel-elevated": "#272822",
    "--border": "#3c3d34",
    "--text": "#f8f8f2",
    "--muted": "#b5b79c",
    "--subtle": "#75715e",
    "--accent": "#66d9ef",
    "--accent-hover": "#8be9fd",
    "--editor-bg": "#1e1f1c",
    "--editor-text": "#f8f8f2",
    "--log-bg": "#181915",
    "--status-bg": "#141510",
    "--toolbar-grad-start": "#2d2e29",
    "--toolbar-grad-end": "#1e1f1c",
    "--tab-bg": "#2a2b26",
    "--tab-active-bg": "#3a3b34",
    "--tab-active-border": "#5b5d4d",
    "--row-hover": "rgba(166, 226, 46, 0.08)",
    "--gutter-bg": "#161712",
    "--current-line-bg": "rgba(255, 255, 255, 0.05)",
    "--token-keyword": "#f92672",
    "--token-string": "#a6e22e",
    "--token-number": "#fd971f",
    "--token-comment": "#75715e",
    "--token-punctuation": "#f8f8f2",
    "--token-decorator": "#fd971f",
    "--token-function": "#66d9ef",
    "--token-type": "#a6e22e",
  },
  "solarized-dark": {
    "--window-bg": "#002b36",
    "--panel-bg": "#073642",
    "--panel-elevated": "#0b3c49",
    "--border": "#184b57",
    "--text": "#eee8d5",
    "--muted": "#93a1a1",
    "--subtle": "#657b83",
    "--accent": "#268bd2",
    "--accent-hover": "#3ea0e6",
    "--editor-bg": "#002b36",
    "--editor-text": "#fdf6e3",
    "--log-bg": "#03252e",
    "--status-bg": "#001f27",
    "--toolbar-grad-start": "#0b3c49",
    "--toolbar-grad-end": "#073642",
    "--tab-bg": "#0c3b47",
    "--tab-active-bg": "#104a58",
    "--tab-active-border": "#2a7181",
    "--row-hover": "rgba(38, 139, 210, 0.1)",
    "--gutter-bg": "#042b34",
    "--current-line-bg": "rgba(255, 255, 255, 0.04)",
    "--token-keyword": "#b58900",
    "--token-string": "#2aa198",
    "--token-number": "#d33682",
    "--token-comment": "#586e75",
    "--token-punctuation": "#93a1a1",
    "--token-decorator": "#cb4b16",
    "--token-function": "#268bd2",
    "--token-type": "#6c71c4",
  },
  "vs-light": {
    "--window-bg": "#f3f5f7",
    "--panel-bg": "#ffffff",
    "--panel-elevated": "#fcfcfc",
    "--border": "#d0d7de",
    "--text": "#1f2328",
    "--muted": "#57606a",
    "--subtle": "#6e7781",
    "--accent": "#0969da",
    "--accent-hover": "#218bff",
    "--editor-bg": "#ffffff",
    "--editor-text": "#1f2328",
    "--log-bg": "#f6f8fa",
    "--status-bg": "#eaeef2",
    "--toolbar-grad-start": "#ffffff",
    "--toolbar-grad-end": "#f3f5f7",
    "--tab-bg": "#eef2f6",
    "--tab-active-bg": "#ffffff",
    "--tab-active-border": "#9ecbff",
    "--row-hover": "rgba(9, 105, 218, 0.08)",
    "--gutter-bg": "#f6f8fa",
    "--current-line-bg": "rgba(9, 105, 218, 0.08)",
    "--token-keyword": "#af00db",
    "--token-string": "#0a7d2c",
    "--token-number": "#b35c00",
    "--token-comment": "#6a737d",
    "--token-punctuation": "#57606a",
    "--token-decorator": "#d73a49",
    "--token-function": "#795e26",
    "--token-type": "#267f99",
  },
};

const escapeHtml = (text) => (text || "").replaceAll("&", "&amp;").replaceAll("<", "&lt;").replaceAll(">", "&gt;");
const basename = (path) => (path || "").split(/[\\/]/).pop() || path;
const getDoc = (path = state.activePath) => state.docs.find((doc) => doc.path === path) || null;
const activeDoc = () => getDoc();
const tokenSpan = (className, text) => `<span class="${className}">${escapeHtml(text)}</span>`;

const PYTHON_DEF_RE = /\b(def)\b([ \t]+)([A-Za-z_][A-Za-z0-9_]*)/y;
const PYTHON_CLASS_RE = /\b(class)\b([ \t]+)([A-Za-z_][A-Za-z0-9_]*)/y;
const PYTHON_DECORATOR_RE = /@[A-Za-z_][A-Za-z0-9_.]*/y;
const PYTHON_TRIPLE_STRING_RE = /("""[\s\S]*?"""|'''[\s\S]*?''')/y;
const PYTHON_STRING_RE = /("(?:[^"\\\n]|\\[\s\S])*"|'(?:[^'\\\n]|\\[\s\S])*')/y;
const PYTHON_KEYWORD_RE = /\b(?:import|from|return|if|elif|else|for|while|try|except|with|as|pass|yield|async|await|True|False|None|raise|lambda|break|continue|global|nonlocal|assert|del|finally|in|is|not|or|and)\b/y;

const ERIRE_QUALIFIED_RE = /\b(screen|page|component|event|var|text|math|button|input|card|webview|py|app)((?:\.[A-Za-z_][A-Za-z0-9_]*)+)/y;
const ERIRE_KEYWORD_RE = /\b(?:import|screen|page|component|event|var|text|math|button|input|card|webview|py|app)\b/y;
const ERIRE_VARIABLE_RE = /\$[A-Za-z_][A-Za-z0-9_.]*/y;
const ERIRE_STRING_RE = /("(?:[^"\\\n]|\\[\s\S])*"|'(?:[^'\\\n]|\\[\s\S])*')/y;

const JSON_KEY_RE = /"(?:[^"\\\n]|\\[\s\S])*"(?=\s*:)/y;
const JSON_STRING_RE = /"(?:[^"\\\n]|\\[\s\S])*"/y;
const JSON_KEYWORD_RE = /\b(?:true|false|null)\b/y;
const NUMBER_RE = /-?\d+(?:\.\d+)?/y;
const PUNCTUATION_RE = /[.[\]{}(),:;]/y;

function matchAt(regex, text, index) {
  regex.lastIndex = index;
  const match = regex.exec(text);
  return match && match.index === index ? match : null;
}

function readToLineEnd(text, index) {
  let end = index;
  while (end < text.length && text[end] !== "\n") {
    end += 1;
  }
  return text.slice(index, end);
}

function highlightPython(text) {
  let index = 0;
  let html = "";

  while (index < text.length) {
    if (text[index] === "#") {
      const comment = readToLineEnd(text, index);
      html += tokenSpan("editor-token-comment", comment);
      index += comment.length;
      continue;
    }

    const tripleString = matchAt(PYTHON_TRIPLE_STRING_RE, text, index);
    if (tripleString) {
      html += tokenSpan("editor-token-string", tripleString[0]);
      index += tripleString[0].length;
      continue;
    }

    const string = matchAt(PYTHON_STRING_RE, text, index);
    if (string) {
      html += tokenSpan("editor-token-string", string[0]);
      index += string[0].length;
      continue;
    }

    const definition = matchAt(PYTHON_DEF_RE, text, index);
    if (definition) {
      html += tokenSpan("editor-token-keyword", definition[1]);
      html += escapeHtml(definition[2]);
      html += tokenSpan("editor-token-function", definition[3]);
      index += definition[0].length;
      continue;
    }

    const classDefinition = matchAt(PYTHON_CLASS_RE, text, index);
    if (classDefinition) {
      html += tokenSpan("editor-token-keyword", classDefinition[1]);
      html += escapeHtml(classDefinition[2]);
      html += tokenSpan("editor-token-type", classDefinition[3]);
      index += classDefinition[0].length;
      continue;
    }

    const decorator = matchAt(PYTHON_DECORATOR_RE, text, index);
    if (decorator) {
      html += tokenSpan("editor-token-decorator", decorator[0]);
      index += decorator[0].length;
      continue;
    }

    const keyword = matchAt(PYTHON_KEYWORD_RE, text, index);
    if (keyword) {
      html += tokenSpan("editor-token-keyword", keyword[0]);
      index += keyword[0].length;
      continue;
    }

    const number = matchAt(NUMBER_RE, text, index);
    if (number) {
      html += tokenSpan("editor-token-number", number[0]);
      index += number[0].length;
      continue;
    }

    const punctuation = matchAt(PUNCTUATION_RE, text, index);
    if (punctuation) {
      html += tokenSpan("editor-token-punctuation", punctuation[0]);
      index += punctuation[0].length;
      continue;
    }

    html += escapeHtml(text[index]);
    index += 1;
  }

  return html;
}

function highlightErire(text) {
  let index = 0;
  let html = "";

  while (index < text.length) {
    if (text[index] === "#") {
      const comment = readToLineEnd(text, index);
      html += tokenSpan("editor-token-comment", comment);
      index += comment.length;
      continue;
    }

    const string = matchAt(ERIRE_STRING_RE, text, index);
    if (string) {
      html += tokenSpan("editor-token-string", string[0]);
      index += string[0].length;
      continue;
    }

    const variable = matchAt(ERIRE_VARIABLE_RE, text, index);
    if (variable) {
      html += tokenSpan("editor-token-variable", variable[0]);
      index += variable[0].length;
      continue;
    }

    const qualified = matchAt(ERIRE_QUALIFIED_RE, text, index);
    if (qualified) {
      html += tokenSpan("editor-token-keyword", qualified[1]);
      for (const part of qualified[2].split(".").filter(Boolean)) {
        html += tokenSpan("editor-token-punctuation", ".");
        html += tokenSpan("editor-token-function", part);
      }
      index += qualified[0].length;
      continue;
    }

    const keyword = matchAt(ERIRE_KEYWORD_RE, text, index);
    if (keyword) {
      html += tokenSpan("editor-token-keyword", keyword[0]);
      index += keyword[0].length;
      continue;
    }

    const number = matchAt(NUMBER_RE, text, index);
    if (number) {
      html += tokenSpan("editor-token-number", number[0]);
      index += number[0].length;
      continue;
    }

    const punctuation = matchAt(PUNCTUATION_RE, text, index);
    if (punctuation) {
      html += tokenSpan("editor-token-punctuation", punctuation[0]);
      index += punctuation[0].length;
      continue;
    }

    html += escapeHtml(text[index]);
    index += 1;
  }

  return html;
}

function highlightJson(text) {
  let index = 0;
  let html = "";

  while (index < text.length) {
    const key = matchAt(JSON_KEY_RE, text, index);
    if (key) {
      html += tokenSpan("editor-token-keyword", key[0]);
      index += key[0].length;
      continue;
    }

    const string = matchAt(JSON_STRING_RE, text, index);
    if (string) {
      html += tokenSpan("editor-token-string", string[0]);
      index += string[0].length;
      continue;
    }

    const keyword = matchAt(JSON_KEYWORD_RE, text, index);
    if (keyword) {
      html += tokenSpan("editor-token-keyword", keyword[0]);
      index += keyword[0].length;
      continue;
    }

    const number = matchAt(NUMBER_RE, text, index);
    if (number) {
      html += tokenSpan("editor-token-number", number[0]);
      index += number[0].length;
      continue;
    }

    const punctuation = matchAt(PUNCTUATION_RE, text, index);
    if (punctuation) {
      html += tokenSpan("editor-token-punctuation", punctuation[0]);
      index += punctuation[0].length;
      continue;
    }

    html += escapeHtml(text[index]);
    index += 1;
  }

  return html;
}

function applyTheme(themeName) {
  const theme = themePresets[themeName] || themePresets["dark-2026"];
  Object.entries(theme).forEach(([name, value]) => {
    document.documentElement.style.setProperty(name, value);
  });
  document.documentElement.style.colorScheme = themeName === "vs-light" ? "light" : "dark";
}

function lineNumbersText(text) {
  const count = Math.max(1, (text.match(/\n/g) || []).length + 1);
  let buffer = "";
  for (let index = 1; index <= count; index += 1) {
    buffer += index;
    if (index !== count) buffer += "\n";
  }
  return buffer;
}

function syncEditorScroll() {
  ui.highlight.style.transform = `translate(${-ui.input.scrollLeft}px, ${-ui.input.scrollTop}px)`;
  ui.gutter.scrollTop = ui.input.scrollTop;
}

function renderCurrentLine(doc = activeDoc()) {
  const lineHeight = parseFloat(getComputedStyle(ui.input).lineHeight) || (state.settings.fontSize * 1.6);
  if (!doc) return;
  ui.currentLine.style.transform = `translateY(${((doc.line || 1) - 1) * lineHeight - ui.input.scrollTop}px)`;
}

function persistDoc() {
  const doc = activeDoc();
  if (!doc) return;
  doc.content = ui.input.value;
  doc.scrollTop = ui.input.scrollTop;
  doc.scrollLeft = ui.input.scrollLeft;
  doc.selectionStart = ui.input.selectionStart;
  doc.selectionEnd = ui.input.selectionEnd;
  const prefix = doc.content.slice(0, doc.selectionStart || 0);
  doc.line = prefix.split("\n").length;
  doc.column = (doc.selectionStart || 0) - prefix.lastIndexOf("\n");
}

function tokenize(doc) {
  if (!doc) return "";
  if ((doc.content || "").length > MAX_HIGHLIGHT_LENGTH) {
    return escapeHtml(doc.content || "");
  }
  const text = doc.content || "";
  if (doc.language === "python") return highlightPython(text);
  if (doc.language === "erire") return highlightErire(text);
  if (doc.language === "json") return highlightJson(text);
  return escapeHtml(text);
}

function updateOutline(doc) {
  if (!doc) return [];
  const lines = doc.content.split(/\r?\n/);
  if (lines.length > MAX_OUTLINE_LINES) {
    doc.outline = [];
    return doc.outline;
  }
  const symbols = [];
  lines.forEach((line, index) => {
    const t = line.trim();
    if (!t) return;
    if (doc.language === "python") {
      if (t.startsWith("def ")) symbols.push({ name: t.slice(4).split("(")[0], kind: "function", line: index + 1 });
      else if (t.startsWith("class ")) symbols.push({ name: t.slice(6).split("(")[0].replace(":", ""), kind: "class", line: index + 1 });
      else if (t.startsWith("import ") || t.startsWith("from ")) symbols.push({ name: t, kind: "import", line: index + 1 });
    } else if (doc.language === "erire") {
      if (t.startsWith("import")) symbols.push({ name: t, kind: "import", line: index + 1 });
      else if (t.startsWith("page ")) symbols.push({ name: t.slice(5), kind: "page", line: index + 1 });
      else if (t.startsWith("component ")) symbols.push({ name: t.slice(10), kind: "component", line: index + 1 });
      else if (t.startsWith("screen.") || t.startsWith("event.") || t.startsWith("var.set")) symbols.push({ name: t, kind: "command", line: index + 1 });
    }
  });
  doc.outline = symbols;
  return symbols;
}

function snapshotFromDoc(doc) {
  return {
    content: doc?.content || "",
    selectionStart: doc?.selectionStart || 0,
    selectionEnd: doc?.selectionEnd || 0,
    scrollTop: doc?.scrollTop || 0,
    scrollLeft: doc?.scrollLeft || 0,
  };
}

function snapshotFromInput() {
  return {
    content: ui.input.value || "",
    selectionStart: ui.input.selectionStart || 0,
    selectionEnd: ui.input.selectionEnd || 0,
    scrollTop: ui.input.scrollTop || 0,
    scrollLeft: ui.input.scrollLeft || 0,
  };
}

function snapshotsEqual(left, right) {
  return !!left && !!right &&
    left.content === right.content &&
    left.selectionStart === right.selectionStart &&
    left.selectionEnd === right.selectionEnd &&
    left.scrollTop === right.scrollTop &&
    left.scrollLeft === right.scrollLeft;
}

function ensureEditorHistory(doc) {
  if (!doc) return;
  if (!Array.isArray(doc.undoStack)) doc.undoStack = [];
  if (!Array.isArray(doc.redoStack)) doc.redoStack = [];
}

function resetEditorHistory(doc) {
  if (!doc) return;
  doc.undoStack = [];
  doc.redoStack = [];
}

function pushUndoSnapshot(doc, snapshot, nextContent = ui.input.value) {
  if (!doc || !snapshot || snapshot.content === nextContent) return;
  ensureEditorHistory(doc);
  const last = doc.undoStack[doc.undoStack.length - 1];
  if (!snapshotsEqual(last, snapshot)) {
    doc.undoStack.push({ ...snapshot });
    if (doc.undoStack.length > EDITOR_HISTORY_LIMIT) {
      doc.undoStack.shift();
    }
  }
  doc.redoStack = [];
}

function restoreEditorSnapshot(doc, snapshot) {
  if (!doc || !snapshot) return;
  doc.content = snapshot.content;
  doc.dirty = true;
  doc.selectionStart = Math.min(snapshot.selectionStart, doc.content.length);
  doc.selectionEnd = Math.min(snapshot.selectionEnd, doc.content.length);
  doc.scrollTop = snapshot.scrollTop;
  doc.scrollLeft = snapshot.scrollLeft;

  ui.input.value = doc.content;
  ui.input.selectionStart = doc.selectionStart;
  ui.input.selectionEnd = doc.selectionEnd;
  ui.input.scrollTop = doc.scrollTop;
  ui.input.scrollLeft = doc.scrollLeft;

  persistDoc();
  scheduleEditorPaint();
  renderTabs();
  renderStatus();
  scheduleOutlineRender();
  scheduleAutosave();
  hideCompletionPanel();
}

function undoEditor() {
  const doc = activeDoc();
  if (!doc) return false;
  ensureEditorHistory(doc);
  const previous = doc.undoStack.pop();
  if (!previous) return false;
  doc.redoStack.push(snapshotFromInput());
  if (doc.redoStack.length > EDITOR_HISTORY_LIMIT) {
    doc.redoStack.shift();
  }
  restoreEditorSnapshot(doc, previous);
  return true;
}

function redoEditor() {
  const doc = activeDoc();
  if (!doc) return false;
  ensureEditorHistory(doc);
  const next = doc.redoStack.pop();
  if (!next) return false;
  doc.undoStack.push(snapshotFromInput());
  if (doc.undoStack.length > EDITOR_HISTORY_LIMIT) {
    doc.undoStack.shift();
  }
  restoreEditorSnapshot(doc, next);
  return true;
}

function getCompletionSource(doc) {
  if (!doc) return [];
  if (doc.language === "python") return PYTHON_COMPLETIONS;
  if (doc.language === "json") return JSON_COMPLETIONS;
  return ERIRE_COMPLETIONS;
}

function getDocumentCompletionItems(doc) {
  if (!doc) return [];
  const items = [];
  const text = doc.content || "";

  if (doc.language === "erire") {
    const variablePattern = /var\.set\[\s*"([^"]+)"/g;
    const bindPattern = /\.bind\[\s*"([^"]+)"/g;
    const idPattern = /\.id\[\s*"([^"]+)"/g;
    for (const pattern of [variablePattern, bindPattern]) {
      let match = pattern.exec(text);
      while (match) {
        items.push({ label: `$${match[1]}`, insert: `$${match[1]}`, detail: "Variable" });
        match = pattern.exec(text);
      }
    }
    let idMatch = idPattern.exec(text);
    while (idMatch) {
      items.push({ label: idMatch[1], insert: idMatch[1], detail: "Node id" });
      idMatch = idPattern.exec(text);
    }
  }

  if (doc.language === "python") {
    const symbolPattern = /^\s*(?:def|class)\s+([A-Za-z_][A-Za-z0-9_]*)/gm;
    let match = symbolPattern.exec(text);
    while (match) {
      items.push({ label: match[1], insert: match[1], detail: "Symbol" });
      match = symbolPattern.exec(text);
    }
  }

  return items;
}

function getCompletionContext() {
  const position = ui.input.selectionStart || 0;
  const before = ui.input.value.slice(0, position);
  const match = before.match(/[A-Za-z_$][A-Za-z0-9_.$]*$/);
  const prefix = match ? match[0] : "";
  return {
    prefix,
    start: position - prefix.length,
    end: position,
  };
}

function matchesCompletion(item, prefix) {
  const needle = prefix.toLowerCase();
  const label = item.label.toLowerCase();
  if (!needle) return true;
  return label.startsWith(needle) || label.includes(needle) || item.insert.toLowerCase().startsWith(needle);
}

function uniqueCompletionItems(items) {
  const seen = new Set();
  return items.filter((item) => {
    const key = `${item.label}\n${item.insert}`;
    if (seen.has(key)) return false;
    seen.add(key);
    return true;
  });
}

function getCompletionItems(doc, prefix, explicit = false) {
  const source = [...getDocumentCompletionItems(doc), ...getCompletionSource(doc)];
  const filtered = uniqueCompletionItems(source)
    .filter((item) => explicit || matchesCompletion(item, prefix))
    .sort((left, right) => {
      const needle = prefix.toLowerCase();
      const leftStarts = left.label.toLowerCase().startsWith(needle) ? 0 : 1;
      const rightStarts = right.label.toLowerCase().startsWith(needle) ? 0 : 1;
      return leftStarts - rightStarts || left.label.localeCompare(right.label);
    });
  return filtered.slice(0, 12);
}

function hideCompletionPanel() {
  completionState.visible = false;
  completionState.items = [];
  completionState.selected = 0;
  if (ui.completionPanel) {
    ui.completionPanel.classList.add("hidden");
    ui.completionPanel.innerHTML = "";
  }
}

function positionCompletionPanel() {
  if (!ui.completionPanel || !ui.scroller) return;
  const position = ui.input.selectionStart || 0;
  const before = ui.input.value.slice(0, position);
  const lineIndex = before.split("\n").length - 1;
  const column = position - before.lastIndexOf("\n") - 1;
  const style = getComputedStyle(ui.input);
  const fontSize = parseFloat(style.fontSize) || state.settings.fontSize || 14;
  const lineHeight = parseFloat(style.lineHeight) || fontSize * 1.6;
  const charWidth = fontSize * 0.62;
  const panelWidth = Math.min(340, Math.max(260, ui.scroller.clientWidth - 16));
  const panelHeight = Math.min(260, Math.max(140, ui.scroller.clientHeight - 16));
  const leftLimit = Math.max(8, ui.scroller.clientWidth - panelWidth - 8);
  const topLimit = Math.max(8, ui.scroller.clientHeight - panelHeight - 8);
  const left = Math.max(8, Math.min(18 + column * charWidth - ui.input.scrollLeft, leftLimit));
  const top = Math.max(8, Math.min(14 + (lineIndex + 1) * lineHeight - ui.input.scrollTop, topLimit));

  ui.completionPanel.style.width = `${panelWidth}px`;
  ui.completionPanel.style.maxHeight = `${panelHeight}px`;
  ui.completionPanel.style.left = `${left}px`;
  ui.completionPanel.style.top = `${top}px`;
}

function renderCompletionPanel() {
  if (!ui.completionPanel) return;
  ui.completionPanel.innerHTML = "";
  completionState.items.forEach((item, index) => {
    const row = document.createElement("button");
    row.type = "button";
    row.className = `completion-item${index === completionState.selected ? " active" : ""}`;
    row.setAttribute("role", "option");
    row.setAttribute("aria-selected", index === completionState.selected ? "true" : "false");

    const label = document.createElement("span");
    label.className = "completion-label";
    label.textContent = item.label;

    const detail = document.createElement("span");
    detail.className = "completion-detail";
    detail.textContent = item.detail || "";

    row.appendChild(label);
    row.appendChild(detail);
    row.addEventListener("mousedown", (event) => {
      event.preventDefault();
      completionState.selected = index;
      applyCompletion();
    });
    ui.completionPanel.appendChild(row);
  });
  ui.completionPanel.classList.toggle("hidden", !completionState.visible || completionState.items.length === 0);
  positionCompletionPanel();
}

function updateCompletionPanel({ explicit = false } = {}) {
  const doc = activeDoc();
  if (!doc || !ui.completionPanel || ui.input.selectionStart !== ui.input.selectionEnd) {
    hideCompletionPanel();
    return false;
  }

  const context = getCompletionContext();
  if (!explicit && context.prefix.length < 1) {
    hideCompletionPanel();
    return false;
  }

  const items = getCompletionItems(doc, context.prefix, explicit);
  if (items.length === 0) {
    hideCompletionPanel();
    return false;
  }

  completionState.visible = true;
  completionState.items = items;
  completionState.selected = 0;
  completionState.prefix = context.prefix;
  completionState.start = context.start;
  completionState.end = context.end;
  renderCompletionPanel();
  return true;
}

function selectCompletion(delta) {
  if (!completionState.visible || completionState.items.length === 0) return;
  const length = completionState.items.length;
  completionState.selected = (completionState.selected + delta + length) % length;
  renderCompletionPanel();
  ui.completionPanel?.querySelector(".completion-item.active")?.scrollIntoView({ block: "nearest" });
}

function markEditorChanged({ updateCompletion = true, undoSnapshot = null } = {}) {
  const doc = activeDoc();
  if (!doc) return;
  if (undoSnapshot) pushUndoSnapshot(doc, undoSnapshot);
  doc.content = ui.input.value;
  doc.dirty = true;
  persistDoc();
  scheduleEditorPaint();
  renderTabs();
  renderStatus();
  scheduleOutlineRender();
  scheduleAutosave();
  if (updateCompletion) updateCompletionPanel();
}

function replaceEditorText(start, end, text, selectionStart, selectionEnd, updateCompletion = true) {
  const value = ui.input.value;
  const undoSnapshot = snapshotFromInput();
  ui.input.value = value.slice(0, start) + text + value.slice(end);
  ui.input.selectionStart = selectionStart;
  ui.input.selectionEnd = selectionEnd;
  markEditorChanged({ updateCompletion, undoSnapshot });
}

function applyCompletion() {
  const item = completionState.items[completionState.selected];
  if (!item) return false;
  const rawInsert = item.insert || item.label;
  const markerIndex = rawInsert.indexOf("|");
  const insert = rawInsert.replace("|", "");
  const caret = markerIndex >= 0 ? completionState.start + markerIndex : completionState.start + insert.length;

  replaceEditorText(completionState.start, completionState.end, insert, caret, caret, false);
  hideCompletionPanel();
  return true;
}

function insertAutoPair(open, close) {
  const start = ui.input.selectionStart || 0;
  const end = ui.input.selectionEnd || start;
  const selected = ui.input.value.slice(start, end);
  const insert = `${open}${selected}${close}`;
  const selectionStart = start + 1;
  const selectionEnd = selected ? end + 1 : start + 1;
  replaceEditorText(start, end, insert, selectionStart, selectionEnd);
}

function skipClosingPair(close) {
  const position = ui.input.selectionStart || 0;
  if (ui.input.selectionEnd !== position || ui.input.value[position] !== close) return false;
  ui.input.selectionStart = position + 1;
  ui.input.selectionEnd = position + 1;
  persistDoc();
  renderCurrentLine(activeDoc());
  renderStatus();
  updateCompletionPanel();
  return true;
}

function deleteEmptyPair() {
  const position = ui.input.selectionStart || 0;
  const value = ui.input.value;
  const pairs = { "(": ")", "[": "]", "{": "}" };
  const open = value[position - 1];
  const close = value[position];
  if (ui.input.selectionEnd !== position || pairs[open] !== close) return false;
  replaceEditorText(position - 1, position + 1, "", position - 1, position - 1);
  return true;
}

function indentSelection(outdent = false) {
  const value = ui.input.value;
  const start = ui.input.selectionStart || 0;
  const end = ui.input.selectionEnd || start;

  if (start === end && !outdent) {
    replaceEditorText(start, end, "  ", start + 2, start + 2);
    return;
  }

  const lineStart = value.lastIndexOf("\n", start - 1) + 1;
  const nextBreak = value.indexOf("\n", end);
  const lineEnd = nextBreak < 0 ? value.length : nextBreak;
  const block = value.slice(lineStart, lineEnd);
  const lines = block.split("\n");
  let firstLineDelta = 0;
  let totalDelta = 0;

  const changed = lines.map((line, index) => {
    if (!outdent) {
      totalDelta += 2;
      if (index === 0) firstLineDelta = 2;
      return `  ${line}`;
    }

    let removeCount = 0;
    if (line.startsWith("  ")) removeCount = 2;
    else if (line.startsWith(" ") || line.startsWith("\t")) removeCount = 1;
    totalDelta -= removeCount;
    if (index === 0) firstLineDelta = -Math.min(removeCount, Math.max(0, start - lineStart));
    return line.slice(removeCount);
  }).join("\n");

  const nextStart = Math.max(lineStart, start + firstLineDelta);
  const nextEnd = Math.max(nextStart, end + totalDelta);
  replaceEditorText(lineStart, lineEnd, changed, nextStart, nextEnd);
}

function handleEditorKeydown(event) {
  if (!activeDoc()) return;

  const primary = event.ctrlKey || event.metaKey;
  const key = event.key.toLowerCase();

  if (primary && key === "z") {
    event.preventDefault();
    if (event.shiftKey) redoEditor();
    else undoEditor();
    return;
  }

  if (primary && key === "y") {
    event.preventDefault();
    redoEditor();
    return;
  }

  if ((event.ctrlKey || event.metaKey) && event.key === " ") {
    event.preventDefault();
    updateCompletionPanel({ explicit: true });
    return;
  }

  if (completionState.visible) {
    if (event.key === "ArrowDown") {
      event.preventDefault();
      selectCompletion(1);
      return;
    }
    if (event.key === "ArrowUp") {
      event.preventDefault();
      selectCompletion(-1);
      return;
    }
    if (event.key === "Tab" || event.key === "Enter") {
      event.preventDefault();
      applyCompletion();
      return;
    }
    if (event.key === "Escape") {
      event.preventDefault();
      hideCompletionPanel();
      return;
    }
  }

  if (event.key === "Tab") {
    event.preventDefault();
    if (!event.shiftKey && updateCompletionPanel()) {
      applyCompletion();
      return;
    }
    indentSelection(event.shiftKey);
    return;
  }

  if (event.ctrlKey || event.metaKey || event.altKey) return;

  const pairs = { "(": ")", "[": "]", "{": "}" };
  if (pairs[event.key]) {
    event.preventDefault();
    insertAutoPair(event.key, pairs[event.key]);
    return;
  }

  if ((event.key === ")" || event.key === "]" || event.key === "}") && skipClosingPair(event.key)) {
    event.preventDefault();
    return;
  }

  if (event.key === "Backspace" && deleteEmptyPair()) {
    event.preventDefault();
  }
}

function scheduleEditorPaint() {
  if (editorWork.paintQueued) return;
  editorWork.paintQueued = true;
  window.requestAnimationFrame(() => {
    const doc = activeDoc();
    editorWork.paintQueued = false;
    if (!doc) return;
    ui.highlight.innerHTML = tokenize(doc);
    ui.gutter.textContent = lineNumbersText(doc.content || "");
    renderCurrentLine(doc);
    syncEditorScroll();
  });
}

function scheduleBottomRender() {
  if (editorWork.bottomQueued) return;
  editorWork.bottomQueued = true;
  window.requestAnimationFrame(() => {
    editorWork.bottomQueued = false;
    renderBottom();
  });
}

function scheduleOutlineRender() {
  clearTimeout(editorWork.outlineTimer);
  editorWork.outlineTimer = window.setTimeout(() => {
    const doc = activeDoc();
    if (!doc) return;
    updateOutline(doc);
    renderOutline();
  }, 160);
}

function hideMenuPanels() {
  openMenuName = "";
  document.querySelectorAll("[data-menu-panel]").forEach((panel) => panel.classList.add("hidden"));
  document.querySelectorAll("[data-menu-toggle]").forEach((button) => button.classList.remove("active"));
}

function showMenuPanel(name) {
  openMenuName = name;
  document.querySelectorAll("[data-menu-panel]").forEach((panel) => {
    panel.classList.toggle("hidden", panel.dataset.menuPanel !== name);
  });
  document.querySelectorAll("[data-menu-toggle]").forEach((button) => {
    button.classList.toggle("active", button.dataset.menuToggle === name);
  });
}

function toggleMenuPanel(name) {
  if (openMenuName === name) {
    hideMenuPanels();
    return;
  }
  showMenuPanel(name);
}

function hideContextMenu() {
  ui.contextMenu.classList.add("hidden");
}

function showContextMenu(x, y) {
  ui.contextMenu.classList.remove("hidden");
  ui.contextMenu.style.left = "0px";
  ui.contextMenu.style.top = "0px";
  const bounds = ui.contextMenu.getBoundingClientRect();
  const left = Math.max(8, Math.min(x, window.innerWidth - bounds.width - 8));
  const top = Math.max(8, Math.min(y, window.innerHeight - bounds.height - 8));
  ui.contextMenu.style.left = `${left}px`;
  ui.contextMenu.style.top = `${top}px`;
}

function renderLayout() {
  applyTheme(state.settings.themeName);
  ui.explorerPanel.classList.toggle("hidden", !state.settings.showExplorer);
  ui.outlinePanel.classList.toggle("hidden", !state.settings.showOutline);
  ui.bottomPanel.classList.toggle("hidden", !state.settings.showBottom);
  const codeFont = `"${state.settings.fontName || "Cascadia Code"}", "Cascadia Code", "Consolas", monospace`;
  ui.input.style.fontFamily = codeFont;
  ui.highlight.style.fontFamily = codeFont;
  ui.gutter.style.fontFamily = codeFont;
  ui.input.style.fontSize = `${state.settings.fontSize}px`;
  ui.highlight.style.fontSize = `${state.settings.fontSize}px`;
  ui.gutter.style.fontSize = `${state.settings.fontSize}px`;
}

function renderTemplateHint() {
  if (!ui.templateHint || !ui.newProjectTemplate) return;
  ui.templateHint.textContent = templateHints[ui.newProjectTemplate.value] || templateHints.default;
}

function isTerminalBusy() {
  return state.process !== "idle";
}

function getTerminalCwd() {
  return state.terminalCwd || state.project?.root || "C:\\";
}

function getTerminalPrompt() {
  return `${getTerminalCwd()}>`;
}

function pushTerminalEntry(kind, text) {
  if (text === undefined || text === null) return;
  const value = String(text).replace(/\r/g, "");
  if (!value) return;
  value.split("\n").forEach((line, index, all) => {
    if (line.length === 0 && index === all.length - 1) return;
    state.terminal.push({ kind, text: line.length ? line : " " });
  });
}

function renderTerminal() {
  ui.terminalPrompt.textContent = getTerminalPrompt();
  ui.terminalInput.disabled = isTerminalBusy();
  ui.terminalInput.placeholder = isTerminalBusy() ? "A process is running..." : "Type a command and press Enter";
  ui.terminalHistory.innerHTML = state.terminal.length
    ? state.terminal.map((entry) => `<span class="terminal-line ${entry.kind || "info"}">${escapeHtml(entry.text || "")}</span>`).join("")
    : '<span class="terminal-line info">Terminal ready.</span>';
  window.requestAnimationFrame(() => {
    ui.terminalHistory.scrollTop = ui.terminalHistory.scrollHeight;
  });
}

function syncTerminalCwdFromProject() {
  if (state.project?.root && !state.terminal.length && !state.terminalCwd) {
    state.terminalCwd = state.project.root;
  }
}

function handleTerminalHistoryNavigation(direction) {
  if (!state.terminalInputHistory.length) return;
  if (direction < 0) {
    if (state.terminalHistoryIndex < 0) {
      state.terminalDraft = ui.terminalInput.value;
      state.terminalHistoryIndex = state.terminalInputHistory.length - 1;
    } else if (state.terminalHistoryIndex > 0) {
      state.terminalHistoryIndex -= 1;
    }
    ui.terminalInput.value = state.terminalInputHistory[state.terminalHistoryIndex] || "";
    return;
  }
  if (state.terminalHistoryIndex < 0) return;
  if (state.terminalHistoryIndex < state.terminalInputHistory.length - 1) {
    state.terminalHistoryIndex += 1;
    ui.terminalInput.value = state.terminalInputHistory[state.terminalHistoryIndex] || "";
    return;
  }
  state.terminalHistoryIndex = -1;
  ui.terminalInput.value = state.terminalDraft || "";
}

function executeTerminalCommand(command) {
  const text = (command || "").trim();
  if (!text) return;
  if (isTerminalBusy()) {
    showToast("Stop the current process before starting another terminal command.", "warning", "Terminal busy");
    return;
  }

  state.terminalInputHistory.push(text);
  if (state.terminalInputHistory.length > 100) {
    state.terminalInputHistory.shift();
  }
  state.terminalHistoryIndex = -1;
  state.terminalDraft = "";

  pushTerminalEntry("command", `${getTerminalPrompt()} ${text}`);
  state.bottomTab = "terminal";

  if (/^(cls|clear)$/i.test(text)) {
    state.terminal = [];
    renderBottom();
    return;
  }

  if (/^(pwd)$/i.test(text)) {
    pushTerminalEntry("prompt", getTerminalCwd());
    renderBottom();
    return;
  }

  const cdMatch = text.match(/^(?:cd|chdir)\s*(.*)$/i);
  if (cdMatch) {
    bridge.post("terminal:changeDirectory", {
      cwd: getTerminalCwd(),
      path: cdMatch[1] ? cdMatch[1].trim() : "",
    });
    renderBottom();
    return;
  }

  renderBottom();
  bridge.post("terminal:execute", {
    cwd: getTerminalCwd(),
    command: text,
  });
}

function setVisibleModal(name) {
  Object.entries(modalViews).forEach(([key, getNode]) => {
    const node = getNode();
    if (node) {
      node.classList.toggle("hidden", key !== name);
    }
  });
  ui.backdrop.classList.toggle("hidden", !name);
}

function showToast(message, kind = "info", title = "") {
  if (!ui.toastHost || !message) return;
  const toast = document.createElement("div");
  toast.className = `toast ${kind}`;

  const titleNode = document.createElement("div");
  titleNode.className = "toast-title";
  titleNode.textContent = title || kind;

  const messageNode = document.createElement("div");
  messageNode.className = "toast-message";
  messageNode.textContent = message;

  toast.appendChild(titleNode);
  toast.appendChild(messageNode);
  ui.toastHost.appendChild(toast);

  window.setTimeout(() => {
    toast.remove();
  }, 3200);
}

function trimText(text, maxChars) {
  const value = String(text || "").replace(/\0/g, "").trim();
  if (value.length <= maxChars) return value;
  return `${value.slice(0, maxChars).trimEnd()}\n...`;
}

function loadAiSettings() {
  try {
    const raw = localStorage.getItem(AI_STORAGE_KEY);
    if (!raw) return;
    const data = JSON.parse(raw);
    if (data && typeof data === "object") {
      state.ai.endpoint = data.endpoint || state.ai.endpoint;
      state.ai.productKey = data.productKey || "";
      state.ai.includeFile = data.includeFile !== false;
      state.ai.includeTree = data.includeTree !== false;
      state.ai.includeProblems = data.includeProblems !== false;
    }
  } catch {
    // Local storage is optional inside WebView2.
  }
}

function saveAiSettingsFromInputs() {
  const previousKey = state.ai.productKey;
  state.ai.endpoint = (ui.aiEndpoint?.value || state.ai.endpoint).trim() || "https://erire.pythonanywhere.com/api/erire-ai-ide";
  state.ai.productKey = (ui.aiProductKey?.value || "").trim();
  if (state.ai.productKey !== previousKey) state.ai.keyPreview = "";
  state.ai.includeFile = !!ui.aiIncludeFile?.checked;
  state.ai.includeTree = !!ui.aiIncludeTree?.checked;
  state.ai.includeProblems = !!ui.aiIncludeProblems?.checked;
  try {
    localStorage.setItem(AI_STORAGE_KEY, JSON.stringify({
      endpoint: state.ai.endpoint,
      productKey: state.ai.productKey,
      includeFile: state.ai.includeFile,
      includeTree: state.ai.includeTree,
      includeProblems: state.ai.includeProblems,
    }));
  } catch {
    // Ignore storage failures; the current in-memory settings still work.
  }
  renderAiPanel();
}

function renderAiPanel() {
  if (ui.aiEndpoint && document.activeElement !== ui.aiEndpoint) ui.aiEndpoint.value = state.ai.endpoint || "";
  if (ui.aiProductKey && document.activeElement !== ui.aiProductKey && ui.aiProductKey.value !== state.ai.productKey) ui.aiProductKey.value = state.ai.productKey || "";
  if (ui.aiIncludeFile) ui.aiIncludeFile.checked = !!state.ai.includeFile;
  if (ui.aiIncludeTree) ui.aiIncludeTree.checked = !!state.ai.includeTree;
  if (ui.aiIncludeProblems) ui.aiIncludeProblems.checked = !!state.ai.includeProblems;
  if (ui.aiStatus) {
    ui.aiStatus.textContent = state.ai.loading ? "Thinking" : (state.ai.keyPreview || (state.ai.productKey ? "Key saved" : "Needs key"));
  }
  if (ui.aiSend) {
    ui.aiSend.disabled = !!state.ai.loading;
    ui.aiSend.textContent = state.ai.loading ? "Thinking" : "Send";
  }
}

function renderAiInline(value) {
  return escapeHtml(value).replace(/`([^`]+)`/g, "<code>$1</code>");
}

function renderAiTextBlock(text) {
  const blocks = String(text || "").split(/\n{2,}/).filter((block) => block.trim());
  return blocks.map((block) => {
    const lines = block.split("\n").filter((line) => line.trim());
    if (lines.every((line) => /^-\s+/.test(line.trim()))) {
      return `<ul>${lines.map((line) => `<li>${renderAiInline(line.trim().replace(/^-\s+/, ""))}</li>`).join("")}</ul>`;
    }
    if (lines.every((line) => /^\d+\.\s+/.test(line.trim()))) {
      return `<ol>${lines.map((line) => `<li>${renderAiInline(line.trim().replace(/^\d+\.\s+/, ""))}</li>`).join("")}</ol>`;
    }
    if (lines.length === 1 && /^#{1,3}\s+/.test(lines[0].trim())) {
      return `<p><strong>${renderAiInline(lines[0].trim().replace(/^#{1,3}\s+/, ""))}</strong></p>`;
    }
    return `<p>${lines.map((line) => renderAiInline(line)).join("<br>")}</p>`;
  }).join("");
}

function renderAiAssistantContent(content) {
  const codeRegex = /```([a-zA-Z0-9_-]+)?\n?([\s\S]*?)```/g;
  let html = "";
  let lastIndex = 0;
  let match;
  while ((match = codeRegex.exec(content)) !== null) {
    const before = content.slice(lastIndex, match.index);
    if (before.trim()) html += renderAiTextBlock(before);
    const language = match[1] || "text";
    const code = match[2].replace(/\n$/, "");
    html += `
      <div class="studio-ai-code">
        <div class="studio-ai-code-toolbar">
          <span>${escapeHtml(language)}</span>
          <button type="button" data-ai-copy-code>Copy</button>
          <button type="button" data-ai-insert-code>Insert</button>
          <button type="button" data-ai-replace-code>Replace</button>
        </div>
        <pre><code>${escapeHtml(code)}</code></pre>
      </div>
    `;
    lastIndex = codeRegex.lastIndex;
  }
  const after = content.slice(lastIndex);
  if (after.trim()) html += renderAiTextBlock(after);
  return html || `<p>${renderAiInline(content)}</p>`;
}

function sanitizeAiReply(content) {
  return String(content || "")
    .replace(/\bI am a large language model,\s*trained by Google\.?/gi, "I am Erire AI, the official assistant for Erire, created by Yahia Saad.")
    .replace(/\bI am a large language model trained by Google\.?/gi, "I am Erire AI, the official assistant for Erire, created by Yahia Saad.")
    .replace(/\bI'm a large language model,\s*trained by Google\.?/gi, "I'm Erire AI, the official assistant for Erire, created by Yahia Saad.")
    .replace(/\bI'm a large language model trained by Google\.?/gi, "I'm Erire AI, the official assistant for Erire, created by Yahia Saad.")
    .replace(/\btrained by Google\b/gi, "created by Yahia Saad for Erire users");
}

function applyAiCode(code, mode) {
  const doc = activeDoc();
  if (!doc) {
    showToast("Open a file before applying AI code.", "warning", "Erire AI");
    return;
  }
  ui.input.focus();
  if (mode === "replace") {
    replaceEditorText(0, ui.input.value.length, code, code.length, code.length, false);
    showToast(`Replaced ${doc.name}.`, "success", "Erire AI");
    return;
  }
  const start = ui.input.selectionStart || 0;
  const end = ui.input.selectionEnd || start;
  replaceEditorText(start, end, code, start + code.length, start + code.length, false);
  showToast(`Inserted AI code into ${doc.name}.`, "success", "Erire AI");
}

function bindAiCodeActions(root) {
  root.querySelectorAll(".studio-ai-code").forEach((block) => {
    const code = block.querySelector("code")?.textContent || "";
    block.querySelector("[data-ai-copy-code]")?.addEventListener("click", async (event) => {
      const button = event.currentTarget;
      try {
        await navigator.clipboard.writeText(code);
        button.textContent = "Copied";
        window.setTimeout(() => { button.textContent = "Copy"; }, 1200);
      } catch {
        button.textContent = "Select";
      }
    });
    block.querySelector("[data-ai-insert-code]")?.addEventListener("click", () => {
      applyAiCode(code, "insert");
    });
    block.querySelector("[data-ai-replace-code]")?.addEventListener("click", async () => {
      const doc = activeDoc();
      if (!doc) {
        showToast("Open a file before replacing it.", "warning", "Erire AI");
        return;
      }
      const confirmed = await showConfirm({
        title: "Replace active file",
        message: `Replace all content in "${doc.name}" with this AI code block?`,
        confirmLabel: "Replace",
        cancelLabel: "Keep",
        tone: "danger",
      });
      if (confirmed) applyAiCode(code, "replace");
    });
  });
}

function appendAiMessage(role, content, options = {}) {
  if (!ui.aiMessages) return null;
  const item = document.createElement("article");
  item.className = `studio-ai-message ${role}${options.loading ? " loading" : ""}`;
  item.innerHTML = `
    <div class="studio-ai-role">${role === "user" ? "You" : "Erire AI"}</div>
    <div class="studio-ai-body">${role === "assistant" && !options.loading ? renderAiAssistantContent(content) : `<p>${renderAiInline(content)}</p>`}</div>
  `;
  ui.aiMessages.appendChild(item);
  bindAiCodeActions(item);
  ui.aiMessages.scrollTop = ui.aiMessages.scrollHeight;
  return item;
}

function collectAiContext() {
  persistDoc();
  const doc = activeDoc();
  const selection = doc ? (doc.content || "").slice(doc.selectionStart || 0, doc.selectionEnd || 0) : "";
  const context = {
    project: state.project ? {
      name: state.project.name || "",
      root: state.project.root || "",
      entry: state.project.entry || "",
    } : null,
    process: state.process,
    openFiles: state.docs.slice(0, 20).map((item) => ({
      path: item.path,
      language: item.language,
      dirty: !!item.dirty,
    })),
    outputTail: trimText(state.output.join(""), 8000),
    terminalTail: trimText(state.terminal.slice(-40).map((entry) => entry.text || "").join("\n"), 6000),
  };
  if (state.ai.includeFile && doc) {
    context.activeFile = {
      path: doc.path,
      language: doc.language,
      line: doc.line || 1,
      column: doc.column || 1,
      dirty: !!doc.dirty,
      selection: trimText(selection, 10000),
      content: trimText(doc.content || "", AI_MAX_ACTIVE_FILE_CHARS),
    };
  }
  if (state.ai.includeTree) {
    context.projectTree = state.treeEntries.slice(0, 220).map((entry) => ({
      relPath: entry.relPath || entry.name || entry.path,
      kind: entry.kind || "file",
    }));
  }
  if (state.ai.includeProblems) {
    context.problems = state.problems.slice(-30);
  }
  return context;
}

async function sendAiMessage(message) {
  const cleanMessage = (message || "").trim();
  if (!cleanMessage || state.ai.loading) return;
  saveAiSettingsFromInputs();
  if (!state.ai.productKey) {
    showToast("Enter your Erire Key in the AI panel.", "warning", "Erire AI");
    return;
  }
  if (!state.ai.endpoint) {
    showToast("Enter the Erire AI API endpoint.", "warning", "Erire AI");
    return;
  }

  state.bottomTab = "ai";
  renderBottom();
  appendAiMessage("user", cleanMessage);
  const loadingMessage = appendAiMessage("assistant", "Thinking with the current project context...", { loading: true });
  state.ai.loading = true;
  renderAiPanel();

  try {
    const response = await fetch(state.ai.endpoint, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        product_key: state.ai.productKey,
        message: cleanMessage,
        history: state.ai.history.slice(-AI_HISTORY_LIMIT),
        context: collectAiContext(),
      }),
    });
    const data = await response.json().catch(() => ({}));
    if (!response.ok) {
      throw new Error(data.error || data.message || "Erire AI could not answer right now.");
    }
    const reply = sanitizeAiReply(String(data.reply || "").trim());
    if (!reply) throw new Error("Erire AI returned an empty reply.");
    loadingMessage?.remove();
    appendAiMessage("assistant", reply);
    state.ai.history.push({ role: "user", content: cleanMessage });
    state.ai.history.push({ role: "assistant", content: reply });
    while (state.ai.history.length > AI_HISTORY_LIMIT) state.ai.history.shift();
    if (data.key_preview) state.ai.keyPreview = data.key_preview;
  } catch (error) {
    loadingMessage?.remove();
    const messageText = error instanceof Error ? error.message : "Erire AI failed.";
    appendAiMessage("assistant", messageText);
    showToast(messageText, "error", "Erire AI");
  } finally {
    state.ai.loading = false;
    renderAiPanel();
    if (ui.aiInput) {
      ui.aiInput.value = "";
      ui.aiInput.focus();
    }
  }
}

function resolveDialog(confirmed) {
  if (!dialogState.resolver) return false;
  const resolve = dialogState.resolver;
  const kind = dialogState.kind;
  const returnFocus = dialogState.returnFocus;
  dialogState.kind = "";
  dialogState.resolver = null;
  dialogState.returnFocus = null;
  setVisibleModal("");
  if (returnFocus && typeof returnFocus.focus === "function") {
    window.requestAnimationFrame(() => returnFocus.focus());
  }
  if (!confirmed) {
    resolve(null);
    return true;
  }
  resolve(kind === "prompt" ? ui.dialogInput.value : true);
  return true;
}

function showDialog({
  kind = "confirm",
  title = "Confirm",
  message = "",
  confirmLabel = "OK",
  cancelLabel = "Cancel",
  inputLabel = "Value",
  placeholder = "",
  value = "",
  tone = "accent",
} = {}) {
  if (dialogState.resolver) {
    resolveDialog(false);
  }

  dialogState.kind = kind;
  dialogState.returnFocus = document.activeElement instanceof HTMLElement ? document.activeElement : null;
  ui.dialogTitle.textContent = title;
  ui.dialogMessage.textContent = message;
  ui.dialogInputLabel.textContent = inputLabel;
  ui.dialogInput.placeholder = placeholder;
  ui.dialogInput.value = value;
  ui.dialogInputRow.classList.toggle("hidden", kind !== "prompt");
  ui.dialogCancel.textContent = cancelLabel;
  ui.dialogCancel.classList.toggle("hidden", kind === "alert");
  ui.dialogConfirm.textContent = confirmLabel;
  ui.dialogConfirm.classList.toggle("danger", tone === "danger");
  ui.dialogConfirm.classList.toggle("accent", tone !== "danger");

  setVisibleModal("dialog");

  return new Promise((resolve) => {
    dialogState.resolver = resolve;
    window.requestAnimationFrame(() => {
      if (kind === "prompt") {
        ui.dialogInput.focus();
        ui.dialogInput.select();
      } else {
        ui.dialogConfirm.focus();
      }
    });
  });
}

function showConfirm(options) {
  return showDialog({ kind: "confirm", ...options }).then((result) => result === true);
}

function showPrompt(options) {
  return showDialog({ kind: "prompt", ...options });
}

function isSamePathOrChild(path, root) {
  if (!path || !root) return false;
  return path === root || path.startsWith(`${root}\\`) || path.startsWith(`${root}/`);
}

function replacePathPrefix(path, oldPrefix, newPrefix) {
  if (!path || !oldPrefix || !newPrefix) return path;
  if (path === oldPrefix) return newPrefix;
  if (path.startsWith(`${oldPrefix}\\`) || path.startsWith(`${oldPrefix}/`)) {
    return `${newPrefix}${path.slice(oldPrefix.length)}`;
  }
  return path;
}

function getExplorerEntry(path = state.explorerSelectedPath) {
  return state.treeEntries.find((entry) => entry.path === path) || null;
}

function selectExplorerPath(path, focus = true) {
  state.explorerSelectedPath = path || "";
  if (focus) {
    ui.explorer.focus();
  }
}

function cancelExplorerRename() {
  state.explorerRenamePath = "";
  renderExplorer();
}

function beginExplorerRename() {
  const entry = getExplorerEntry();
  if (!entry) return;
  state.explorerRenamePath = entry.path;
  renderExplorer();
}

function commitExplorerRename(value) {
  const entry = getExplorerEntry(state.explorerRenamePath);
  const nextName = (value || "").trim();

  if (!entry) {
    state.explorerRenamePath = "";
    renderExplorer();
    return;
  }
  if (!nextName || nextName === entry.name) {
    state.explorerRenamePath = "";
    renderExplorer();
    return;
  }

  state.explorerRenamePath = "";
  bridge.post("project:renameEntry", { path: entry.path, newName: nextName });
}

async function deleteSelectedExplorerEntry() {
  const entry = getExplorerEntry();
  if (!entry) return;
  const label = entry.kind === "folder" ? "folder" : "file";
  const confirmed = await showConfirm({
    title: `Delete ${label}`,
    message: `Delete ${label} "${entry.name}"? This action cannot be undone from Erire Studio.`,
    confirmLabel: "Delete",
    cancelLabel: "Keep",
    tone: "danger",
  });
  if (!confirmed) {
    return;
  }
  state.explorerRenamePath = "";
  bridge.post("project:deleteEntry", { path: entry.path });
}

async function findInActiveDoc() {
  const doc = activeDoc();
  if (!doc) {
    showToast("Open a file before searching.", "warning", "No document");
    return;
  }
  const selectedText = (doc.content || "").slice(doc.selectionStart || 0, doc.selectionEnd || 0);
  const term = await showPrompt({
    title: "Find text",
    message: "Enter the text to find in the active document.",
    inputLabel: "Find",
    placeholder: "Search term",
    value: selectedText && !selectedText.includes("\n") ? selectedText : "",
    confirmLabel: "Find",
  });
  if (term === null) return;
  if (!term) {
    showToast("Type some text to search for.", "warning", "Empty search");
    return;
  }

  const start = doc.selectionEnd || 0;
  let index = doc.content.indexOf(term, start);
  if (index < 0 && start > 0) {
    index = doc.content.indexOf(term, 0);
  }
  if (index < 0) {
    showToast(`"${term}" was not found in the current file.`, "warning", "No match");
    return;
  }

  ui.input.focus();
  ui.input.selectionStart = index;
  ui.input.selectionEnd = index + term.length;
  persistDoc();
  renderCurrentLine(doc);
  renderStatus();
}

async function replaceInActiveDoc() {
  const doc = activeDoc();
  if (!doc) {
    showToast("Open a file before replacing text.", "warning", "No document");
    return;
  }

  const selectedText = (doc.content || "").slice(doc.selectionStart || 0, doc.selectionEnd || 0);
  const term = await showPrompt({
    title: "Replace text",
    message: "Enter the text you want to replace everywhere in the active document.",
    inputLabel: "Find",
    placeholder: "Search term",
    value: selectedText && !selectedText.includes("\n") ? selectedText : "",
    confirmLabel: "Next",
  });
  if (term === null) return;
  if (!term) {
    showToast("Type some text to replace.", "warning", "Empty search");
    return;
  }

  const replacement = await showPrompt({
    title: "Replace with",
    message: `Choose the replacement text for "${term}".`,
    inputLabel: "Replace with",
    placeholder: "Replacement",
    value: "",
    confirmLabel: "Replace",
  });
  if (replacement === null) return;

  const occurrences = doc.content.split(term).length - 1;
  if (occurrences < 1) {
    showToast(`"${term}" was not found in the current file.`, "warning", "No match");
    return;
  }

  const undoSnapshot = snapshotFromDoc(doc);
  doc.content = doc.content.replaceAll(term, replacement);
  doc.dirty = true;
  pushUndoSnapshot(doc, undoSnapshot, doc.content);
  renderEditor();
  renderTabs();
  scheduleOutlineRender();
  renderStatus();
  scheduleAutosave();
  showToast(`Replaced ${occurrences} occurrence${occurrences === 1 ? "" : "s"} in ${doc.name}.`, "success", "Replace complete");
}

function renderExplorer() {
  if (!state.project) {
    state.explorerSelectedPath = "";
    state.explorerRenamePath = "";
    ui.explorer.innerHTML = '<div class="tree-node">No project loaded.</div>';
    return;
  }
  ui.explorer.innerHTML = "";
  for (const entry of state.treeEntries) {
    const hidden = entry.relPath.split("\\").some((_, i, parts) => i < parts.length - 1 && state.collapsed.has(parts.slice(0, i + 1).join("\\")));
    if (hidden) continue;
    const row = document.createElement("div");
    const isSelected = state.explorerSelectedPath === entry.path;
    row.className = `tree-node${state.activePath === entry.path ? " active" : ""}${isSelected ? " selected" : ""}`;
    row.style.paddingLeft = `${8 + entry.depth * 14}px`;
    row.dataset.path = entry.path;

    const glyph = document.createElement("span");
    glyph.textContent = entry.kind === "folder" ? (state.collapsed.has(entry.relPath) ? ">" : "v") : "";
    row.appendChild(glyph);

    if (state.explorerRenamePath === entry.path) {
      const input = document.createElement("input");
      input.className = "tree-renamer";
      input.type = "text";
      input.value = entry.name;
      input.addEventListener("click", (event) => event.stopPropagation());
      input.addEventListener("keydown", (event) => {
        if (event.key === "Enter") {
          event.preventDefault();
          commitExplorerRename(input.value);
        } else if (event.key === "Escape") {
          event.preventDefault();
          cancelExplorerRename();
        }
      });
      input.addEventListener("blur", () => commitExplorerRename(input.value));
      row.appendChild(input);
      ui.explorer.appendChild(row);
      window.requestAnimationFrame(() => {
        input.focus();
        input.select();
      });
      continue;
    }

    const name = document.createElement("span");
    name.className = "tree-name";
    name.textContent = entry.name;
    row.appendChild(name);
    row.addEventListener("click", () => {
      selectExplorerPath(entry.path);
      if (entry.kind === "folder") {
        if (state.collapsed.has(entry.relPath)) state.collapsed.delete(entry.relPath);
        else state.collapsed.add(entry.relPath);
        renderExplorer();
      } else {
        bridge.post("app:openFile", { path: entry.path });
      }
    });
    ui.explorer.appendChild(row);
  }
}

function renderTabs() {
  ui.tabs.innerHTML = "";
  state.docs.forEach((doc) => {
    const tab = document.createElement("div");
    tab.className = `tab${doc.path === state.activePath ? " active" : ""}`;
    tab.innerHTML = `<span class="tab-title">${escapeHtml(doc.name)}${doc.dirty ? " *" : ""}</span><button class="tab-close">x</button>`;
    tab.addEventListener("click", () => setActive(doc.path));
    tab.querySelector(".tab-close").addEventListener("click", (event) => {
      event.stopPropagation();
      closeDoc(doc.path);
    });
    ui.tabs.appendChild(tab);
  });
}

function renderEditor() {
  const doc = activeDoc();
  ui.welcome.classList.toggle("hidden", !!doc);
  ui.editorShell.classList.toggle("hidden", !doc);
  hideCompletionPanel();
  if (!doc) return;
  ensureEditorHistory(doc);
  ui.input.value = doc.content || "";
  ui.input.scrollTop = doc.scrollTop || 0;
  ui.input.scrollLeft = doc.scrollLeft || 0;
  ui.input.selectionStart = doc.selectionStart || 0;
  ui.input.selectionEnd = doc.selectionEnd || 0;
  persistDoc();
  scheduleEditorPaint();
}

function renderOutline() {
  const doc = activeDoc();
  if (!doc) {
    ui.outline.innerHTML = '<div class="outline-item">No active document.</div>';
    return;
  }
  if (!Array.isArray(doc.outline)) {
    updateOutline(doc);
  }
  ui.outline.innerHTML = doc.outline.length
    ? doc.outline.map((symbol) => `<div class="outline-item" data-line="${symbol.line}"><span>${escapeHtml(symbol.name)}</span><span class="outline-meta">${symbol.kind} : ${symbol.line}</span></div>`).join("")
    : '<div class="outline-item">No symbols detected.</div>';
  ui.outline.querySelectorAll("[data-line]").forEach((node) => {
    node.addEventListener("click", () => {
      const line = Number(node.dataset.line || "1");
      const current = activeDoc();
      if (!current) return;
      const position = current.content.split("\n").slice(0, line - 1).join("\n").length + (line > 1 ? 1 : 0);
      ui.input.focus();
      ui.input.selectionStart = position;
      ui.input.selectionEnd = position;
      persistDoc();
      renderCurrentLine(current);
      renderStatus();
    });
  });
}

function classifyLogLine(line) {
  const lower = line.toLowerCase();
  if (lower.includes("error") || lower.includes("failed") || lower.includes("exception") || lower.includes("traceback") || /\bcode\s+[1-9]\d*/.test(lower)) {
    return "error";
  }
  if (lower.includes("warning")) {
    return "warning";
  }
  if (lower.includes("ok:") || lower.includes("success") || lower.includes("succeeded") || lower.includes("exited with code 0")) {
    return "success";
  }
  return "info";
}

function renderLogHtml(text) {
  const source = text || "";
  const normalized = source.replace(/\r/g, "");
  if (!normalized) return "";
  return normalized.split("\n").map((line) => {
    const safe = line.length ? escapeHtml(line) : "&nbsp;";
    return `<span class="log-line ${classifyLogLine(line)}">${safe}</span>`;
  }).join("");
}

function renderBottom() {
  document.querySelectorAll(".bottom-tab").forEach((button) => {
    button.classList.toggle("active", button.dataset.bottomTab === state.bottomTab);
  });
  document.querySelectorAll(".bottom-view").forEach((view) => {
    view.classList.toggle("active", view.id === `panel-${state.bottomTab}`);
  });
  ui.output.innerHTML = renderLogHtml(state.output.join(""));
  renderTerminal();
  ui.problems.innerHTML = state.problems.length
    ? state.problems.map((problem) => `<div class="problem-item ${problem.kind}"><span class="problem-kind">${problem.kind}</span><span>${escapeHtml(problem.text)}</span><span class="problem-meta">${escapeHtml(problem.source)}</span></div>`).join("")
    : '<div class="problem-item"><span>No problems.</span></div>';
  renderAiPanel();
}

function renderStatus() {
  const doc = activeDoc();
  ui.projectTitle.textContent = state.project?.name || "No Project";
  ui.projectPath.textContent = state.project?.root || "Open or create an Erire project";
  ui.statusFile.textContent = doc?.path || "No file";
  ui.statusProject.textContent = state.project?.name || "No project";
  ui.statusCursor.textContent = doc ? `Ln ${doc.line || 1}, Col ${doc.column || 1}` : "Ln 1, Col 1";
  ui.statusLanguage.textContent = doc?.language || "text";
  ui.statusProcess.textContent = state.process;
  ui.statusSave.textContent = doc?.dirty ? "Modified" : "Saved";
}

function renderAll() {
  renderLayout();
  renderExplorer();
  renderTabs();
  renderEditor();
  renderOutline();
  renderBottom();
  renderStatus();
}

function markProblems(text, source) {
  (text || "").split(/\r?\n/).forEach((line) => {
    const lower = line.toLowerCase();
    if (lower.includes("error") || lower.includes("warning")) {
      state.problems.push({ kind: lower.includes("warning") ? "warning" : "error", source, text: line.trim() });
    }
  });
}

function scheduleAutosave() {
  clearTimeout(state.autosaveTimer);
  if (!state.settings.autosave) return;
  state.autosaveTimer = window.setTimeout(() => saveActive(false), 500);
}

function saveActive(saveAs) {
  const doc = activeDoc();
  if (!doc) return;
  persistDoc();
  bridge.post(saveAs ? "app:saveFileAs" : "app:saveFile", { path: doc.path, content: doc.content });
}

function openModal(name) {
  setVisibleModal(name);
  if (name === "newProject") {
    renderTemplateHint();
  }
  if (name === "settings") {
    ui.settingsErire.value = state.settings.erireExe || "";
    ui.settingsPython.value = state.settings.pythonExe || "";
    ui.settingsFontName.value = state.settings.fontName || "";
    ui.settingsTheme.value = state.settings.themeName || "dark-2026";
    ui.settingsFontSize.value = state.settings.fontSize || 14;
    ui.settingsAutosave.checked = !!state.settings.autosave;
  }
}

function closeModal() {
  if (resolveDialog(false)) {
    return;
  }
  setVisibleModal("");
}

function setActive(path) {
  persistDoc();
  state.activePath = path;
  state.explorerSelectedPath = path || state.explorerSelectedPath;
  if (path) bridge.post("file:setActive", { path });
  renderExplorer();
  renderTabs();
  renderEditor();
  renderOutline();
  renderStatus();
}

function closeDoc(path) {
  const index = state.docs.findIndex((doc) => doc.path === path);
  if (index < 0) return;
  state.docs.splice(index, 1);
  bridge.post("file:closeTab", { path });
  if (state.activePath === path) {
    state.activePath = state.docs[Math.max(index - 1, 0)]?.path || "";
  }
  renderExplorer();
  renderTabs();
  renderEditor();
  renderOutline();
  renderStatus();
}

function execute(command) {
  hideMenuPanels();
  hideContextMenu();
  switch (command) {
    case "newProject":
      openModal("newProject");
      break;
    case "openProject":
      bridge.post("app:openProject");
      break;
    case "openFile":
      bridge.post("app:openFile");
      break;
    case "save":
      saveActive(false);
      break;
    case "saveAs":
      saveActive(true);
      break;
    case "saveAll":
      persistDoc();
      state.docs.filter((doc) => doc.dirty).forEach((doc) => {
        bridge.post("app:saveFile", { path: doc.path, content: doc.content });
      });
      break;
    case "closeProject":
      bridge.post("project:close");
      break;
    case "renameExplorerEntry":
      beginExplorerRename();
      break;
    case "deleteExplorerEntry":
      void deleteSelectedExplorerEntry();
      break;
    case "runProject":
      state.bottomTab = "terminal";
      renderBottom();
      bridge.post("run:project");
      break;
    case "checkProject":
      state.problems = [];
      state.bottomTab = "terminal";
      renderBottom();
      scheduleBottomRender();
      bridge.post("build:check");
      break;
    case "buildProject":
      state.problems = [];
      state.bottomTab = "terminal";
      renderBottom();
      scheduleBottomRender();
      bridge.post("build:project");
      break;
    case "stopProcess":
      bridge.post("process:stop");
      break;
    case "refreshProject":
      bridge.post("project:refresh");
      break;
    case "reloadProject":
      bridge.post("project:refresh");
      break;
    case "openProjectFolder":
      bridge.post("tools:openProjectFolder");
      break;
    case "openErireAI":
      state.bottomTab = "ai";
      renderBottom();
      window.requestAnimationFrame(() => ui.aiInput?.focus());
      break;
    case "openSettings":
      openModal("settings");
      break;
    case "aboutStudio":
      openModal("about");
      break;
    case "openFirstStandStudio":
      bridge.post("app:openExternal", { url: "https://firststandstudio.github.io" });
      break;
    case "exitApp":
      bridge.post("app:exit");
      break;
    case "refreshSymbols":
      updateOutline(activeDoc());
      renderOutline();
      break;
    case "clearOutput":
      state.output = [];
      state.terminal = [];
      state.problems = [];
      renderBottom();
      break;
    case "toggleExplorer":
      state.settings.showExplorer = !state.settings.showExplorer;
      renderLayout();
      renderStatus();
      bridge.post("settings:save", state.settings);
      break;
    case "toggleOutline":
      state.settings.showOutline = !state.settings.showOutline;
      renderLayout();
      renderStatus();
      bridge.post("settings:save", state.settings);
      break;
    case "toggleBottom":
      state.settings.showBottom = !state.settings.showBottom;
      renderLayout();
      renderStatus();
      bridge.post("settings:save", state.settings);
      break;
    case "resetLayout":
      Object.assign(state.settings, { showExplorer: true, showOutline: true, showBottom: true });
      renderLayout();
      renderStatus();
      bridge.post("settings:save", state.settings);
      break;
    case "submitNewProject":
      bridge.post("app:newProject", {
        name: ui.newProjectName.value.trim(),
        location: ui.newProjectLocation.value.trim(),
        template: ui.newProjectTemplate.value,
      });
      closeModal();
      break;
    case "saveSettings":
      state.settings.erireExe = ui.settingsErire.value.trim();
      state.settings.pythonExe = ui.settingsPython.value.trim();
      state.settings.fontName = ui.settingsFontName.value.trim();
      state.settings.themeName = ui.settingsTheme.value;
      state.settings.fontSize = Number(ui.settingsFontSize.value || 14);
      state.settings.autosave = ui.settingsAutosave.checked;
      renderLayout();
      scheduleEditorPaint();
      renderStatus();
      bridge.post("settings:save", state.settings);
      closeModal();
      break;
    case "runPythonFile":
      state.bottomTab = "terminal";
      renderBottom();
      bridge.post("run:python", { path: state.activePath });
      break;
    case "undo":
      ui.input.focus();
      if (!undoEditor()) {
        document.execCommand?.("undo");
      }
      break;
    case "redo":
      ui.input.focus();
      if (!redoEditor()) {
        document.execCommand?.("redo");
      }
      break;
    case "cut":
      ui.input.focus();
      document.execCommand?.("cut");
      break;
    case "copy":
      ui.input.focus();
      document.execCommand?.("copy");
      break;
    case "paste":
      ui.input.focus();
      document.execCommand?.("paste");
      break;
    case "find":
      void findInActiveDoc();
      break;
    case "replace":
      void replaceInActiveDoc();
      break;
    case "selectAll":
      ui.input.focus();
      ui.input.select();
      break;
    default:
      break;
  }
}

function onNative(raw) {
  let message;
  try {
    message = typeof raw === "string" ? JSON.parse(raw) : JSON.parse(raw.data);
  } catch {
    return;
  }

  const { type, payload } = message;

  if (type === "project:loaded") {
    state.project = payload.open === false ? null : payload;
    if (!state.project) {
      state.docs = [];
      state.activePath = "";
      state.explorerSelectedPath = "";
      state.explorerRenamePath = "";
      if (!state.terminalCwd) {
        state.terminalCwd = "C:\\";
      }
    } else if (state.terminalInputHistory.length === 0) {
      state.terminalCwd = payload.root || state.terminalCwd;
    }
    syncTerminalCwdFromProject();
    renderExplorer();
    renderTabs();
    renderEditor();
    renderOutline();
    renderBottom();
    renderStatus();
    return;
  }

  if (type === "project:tree") {
    state.treeEntries = payload.entries || [];
    if (state.explorerSelectedPath && !state.treeEntries.some((entry) => entry.path === state.explorerSelectedPath)) {
      state.explorerSelectedPath = state.activePath && state.treeEntries.some((entry) => entry.path === state.activePath) ? state.activePath : "";
    }
    renderExplorer();
    return;
  }

  if (type === "file:content") {
    const existing = getDoc(payload.path);
    let doc;
    if (existing) {
      Object.assign(existing, {
        content: payload.content || "",
        language: payload.language || "text",
        dirty: false,
        name: basename(payload.path),
      });
      doc = existing;
    } else {
      doc = {
        path: payload.path,
        name: basename(payload.path),
        language: payload.language || "text",
        content: payload.content || "",
        dirty: false,
        line: 1,
        column: 1,
        outline: [],
      };
      state.docs.push(doc);
    }
    resetEditorHistory(doc);
    state.activePath = payload.path;
    state.explorerSelectedPath = payload.path;
    renderExplorer();
    renderTabs();
    renderEditor();
    renderOutline();
    renderStatus();
    return;
  }

  if (type === "project:entryRenamed") {
    const oldName = basename(payload.path);
    const newName = basename(payload.newPath);
    state.docs = state.docs.map((doc) => {
      const nextPath = replacePathPrefix(doc.path, payload.path, payload.newPath);
      return nextPath === doc.path ? doc : { ...doc, path: nextPath, name: basename(nextPath) };
    });
    state.activePath = replacePathPrefix(state.activePath, payload.path, payload.newPath);
    state.explorerSelectedPath = replacePathPrefix(state.explorerSelectedPath, payload.path, payload.newPath);
    state.explorerRenamePath = "";
    renderExplorer();
    renderTabs();
    renderEditor();
    renderStatus();
    showToast(`Renamed ${oldName} to ${newName}.`, "success", "Renamed");
    return;
  }

  if (type === "project:entryDeleted") {
    const removedName = basename(payload.path);
    state.docs = state.docs.filter((doc) => !isSamePathOrChild(doc.path, payload.path));
    if (isSamePathOrChild(state.activePath, payload.path)) {
      state.activePath = state.docs[0]?.path || "";
    }
    if (isSamePathOrChild(state.explorerSelectedPath, payload.path)) {
      state.explorerSelectedPath = state.activePath || "";
    }
    state.explorerRenamePath = "";
    renderExplorer();
    renderTabs();
    renderEditor();
    renderStatus();
    showToast(`Removed ${removedName}.`, "success", "Deleted");
    return;
  }

  if (type === "file:saved") {
    const doc = getDoc(payload.path) || activeDoc();
    if (doc) {
      doc.path = payload.path;
      doc.name = basename(payload.path);
      doc.dirty = false;
    }
    state.activePath = payload.path;
    renderTabs();
    renderStatus();
    return;
  }

  if (type === "outline:data") {
    const doc = getDoc(payload.path);
    if (doc) {
      doc.outline = payload.symbols || [];
      if (doc.path === state.activePath) renderOutline();
    }
    return;
  }

  if (type === "run:output" || type === "build:output") {
    state.output.push(payload.text || "");
    markProblems(payload.text || "", payload.kind || "run");
    scheduleBottomRender();
    return;
  }

  if (type === "terminal:output") {
    state.bottomTab = "terminal";
    pushTerminalEntry(classifyLogLine(payload.text || ""), payload.text || "");
    scheduleBottomRender();
    return;
  }

  if (type === "terminal:cwd") {
    if (payload.cwd) {
      state.terminalCwd = payload.cwd;
      if (payload.message) {
        pushTerminalEntry(payload.kind || "success", payload.message);
      }
      renderBottom();
      window.requestAnimationFrame(() => ui.terminalInput?.focus());
    }
    return;
  }

  if (type === "settings:data") {
    state.settings = { ...state.settings, ...payload };
    renderLayout();
    scheduleEditorPaint();
    renderStatus();
    return;
  }

  if (type === "process:state") {
    state.process = payload.kind === "idle" ? "idle" : `${payload.kind}:${payload.state}`;
    renderBottom();
    renderStatus();
    if (state.process === "idle" && state.bottomTab === "terminal") {
      window.requestAnimationFrame(() => ui.terminalInput?.focus());
    }
    return;
  }

  if (type === "status:update" && typeof payload.exitCode === "number") {
    if (payload.kind === "terminal") {
      pushTerminalEntry(payload.exitCode === 0 ? "success" : "error", `[terminal] exited with code ${payload.exitCode}`);
    } else {
      state.output.push(`\n[${payload.kind}] exited with code ${payload.exitCode}\n`);
    }
    scheduleBottomRender();
    return;
  }

  if (type === "dialog:selected") {
    if (payload.target === "newProjectLocation") ui.newProjectLocation.value = payload.path || "";
    if (payload.target === "settingsErire") ui.settingsErire.value = payload.path || "";
    if (payload.target === "settingsPython") ui.settingsPython.value = payload.path || "";
    return;
  }

  if (type === "command:execute") {
    execute(payload.id);
    return;
  }

  if (type === "app:error") {
    state.output.push(`\n[error] ${payload.message}\n`);
    scheduleBottomRender();
    showToast(payload.message || "An unexpected error occurred.", "error", "Erire Studio");
  }
}

document.querySelectorAll("[data-command]").forEach((button) => {
  button.addEventListener("click", () => execute(button.dataset.command));
});

document.querySelectorAll("[data-menu-toggle]").forEach((button) => {
  button.addEventListener("click", (event) => {
    event.stopPropagation();
    toggleMenuPanel(button.dataset.menuToggle);
  });
  button.addEventListener("mouseenter", () => {
    if (openMenuName) {
      showMenuPanel(button.dataset.menuToggle);
    }
  });
});

document.querySelectorAll("[data-bottom-tab]").forEach((button) => {
  button.addEventListener("click", () => {
    state.bottomTab = button.dataset.bottomTab;
    renderBottom();
    if (state.bottomTab === "terminal" && !isTerminalBusy()) {
      window.requestAnimationFrame(() => ui.terminalInput?.focus());
    }
  });
});

document.querySelectorAll("[data-close-modal]").forEach((button) => {
  button.addEventListener("click", closeModal);
});

document.querySelectorAll("[data-dialog-action]").forEach((button) => {
  button.addEventListener("click", () => {
    resolveDialog(button.dataset.dialogAction === "confirm");
  });
});

document.querySelectorAll("[data-dialog-target]").forEach((button) => {
  button.addEventListener("click", () => {
    bridge.post(button.dataset.dialogTarget === "newProjectLocation" ? "dialog:pickFolder" : "dialog:pickFile", {
      target: button.dataset.dialogTarget,
    });
  });
});

document.querySelectorAll("[data-ai-command]").forEach((button) => {
  button.addEventListener("click", () => {
    if (button.dataset.aiCommand === "saveSettings") {
      saveAiSettingsFromInputs();
      showToast("Erire AI settings saved.", "success", "Erire AI");
    }
    if (button.dataset.aiCommand === "clearChat") {
      state.ai.history = [];
      if (ui.aiMessages) ui.aiMessages.innerHTML = "";
      appendAiMessage("assistant", "Erire AI is ready inside Studio.");
    }
  });
});

document.querySelectorAll("[data-ai-prompt]").forEach((button) => {
  button.addEventListener("click", () => {
    const prompt = button.dataset.aiPrompt || "";
    if (ui.aiInput) ui.aiInput.value = prompt;
    void sendAiMessage(prompt);
  });
});

ui.aiForm?.addEventListener("submit", (event) => {
  event.preventDefault();
  void sendAiMessage(ui.aiInput?.value || "");
});

ui.aiInput?.addEventListener("keydown", (event) => {
  if (event.key === "Enter" && !event.shiftKey) {
    event.preventDefault();
    ui.aiForm?.requestSubmit();
  }
});

[ui.aiEndpoint, ui.aiProductKey, ui.aiIncludeFile, ui.aiIncludeTree, ui.aiIncludeProblems].forEach((node) => {
  node?.addEventListener("change", saveAiSettingsFromInputs);
});

ui.newProjectTemplate?.addEventListener("change", renderTemplateHint);
ui.backdrop.addEventListener("mousedown", (event) => {
  if (event.target === ui.backdrop) {
    closeModal();
  }
});
ui.dialogInput?.addEventListener("keydown", (event) => {
  if (event.key === "Enter") {
    event.preventDefault();
    resolveDialog(true);
    return;
  }
  if (event.key === "Escape") {
    event.preventDefault();
    resolveDialog(false);
  }
});
ui.terminalInput?.addEventListener("keydown", (event) => {
  if (event.key === "Enter") {
    event.preventDefault();
    const command = ui.terminalInput.value;
    ui.terminalInput.value = "";
    executeTerminalCommand(command);
    return;
  }
  if (event.key === "ArrowUp") {
    event.preventDefault();
    handleTerminalHistoryNavigation(-1);
    return;
  }
  if (event.key === "ArrowDown") {
    event.preventDefault();
    handleTerminalHistoryNavigation(1);
  }
});

ui.explorer.addEventListener("keydown", (event) => {
  if (state.explorerRenamePath) return;
  if (event.key === "F2") {
    event.preventDefault();
    beginExplorerRename();
    return;
  }
  if (event.key === "Delete") {
    event.preventDefault();
    void deleteSelectedExplorerEntry();
    return;
  }
  if (event.key === "Enter") {
    const entry = getExplorerEntry();
    if (!entry) return;
    event.preventDefault();
    if (entry.kind === "folder") {
      if (state.collapsed.has(entry.relPath)) state.collapsed.delete(entry.relPath);
      else state.collapsed.add(entry.relPath);
      renderExplorer();
    } else {
      bridge.post("app:openFile", { path: entry.path });
    }
  }
});

ui.input.addEventListener("input", () => {
  markEditorChanged({ undoSnapshot: snapshotFromDoc(activeDoc()) });
});

const COMPLETION_NAVIGATION_KEYS = new Set(["ArrowUp", "ArrowDown", "Enter", "Tab", "Escape"]);

["click", "keyup"].forEach((eventName) => {
  ui.input.addEventListener(eventName, (event) => {
    const doc = activeDoc();
    persistDoc();
    renderCurrentLine(doc);
    renderStatus();
    if (eventName === "keyup" && COMPLETION_NAVIGATION_KEYS.has(event.key)) return;
    updateCompletionPanel();
  });
});

ui.input.addEventListener("keydown", handleEditorKeydown);

ui.input.addEventListener("scroll", () => {
  syncEditorScroll();
  const doc = activeDoc();
  if (doc) {
    doc.scrollTop = ui.input.scrollTop;
    doc.scrollLeft = ui.input.scrollLeft;
  }
  renderCurrentLine(doc);
  if (completionState.visible) positionCompletionPanel();
});

ui.input.addEventListener("contextmenu", (event) => {
  event.preventDefault();
  ui.input.focus();
  showContextMenu(event.clientX, event.clientY);
});

document.addEventListener("mousedown", (event) => {
  if (!event.target.closest(".menu-root")) {
    hideMenuPanels();
  }
  if (!event.target.closest(".context-menu")) {
    hideContextMenu();
  }
  if (!event.target.closest(".completion-panel") && event.target !== ui.input) {
    hideCompletionPanel();
  }
});

window.addEventListener("blur", () => {
  hideMenuPanels();
  hideContextMenu();
  hideCompletionPanel();
});

window.addEventListener("keydown", (event) => {
  if (event.defaultPrevented) return;
  if (event.key === "Escape") {
    hideMenuPanels();
    hideContextMenu();
    hideCompletionPanel();
    closeModal();
    return;
  }
  const key = event.key.toLowerCase();
  const primary = event.ctrlKey || event.metaKey;
  const editingField = event.target instanceof HTMLElement &&
    ["INPUT", "TEXTAREA", "SELECT"].includes(event.target.tagName) &&
    event.target !== ui.input;

  if (primary && key === "z" && !editingField) {
    event.preventDefault();
    if (event.shiftKey) redoEditor();
    else undoEditor();
    return;
  }
  if (primary && key === "y" && !editingField) {
    event.preventDefault();
    redoEditor();
    return;
  }

  if (primary && key === "n") {
    event.preventDefault();
    execute("newProject");
    return;
  }
  if (primary && key === "o" && event.shiftKey) {
    event.preventDefault();
    execute("openFile");
    return;
  }
  if (primary && key === "o") {
    event.preventDefault();
    execute("openProject");
    return;
  }
  if (primary && key === "s") {
    event.preventDefault();
    saveActive(event.shiftKey);
    return;
  }
  if (primary && key === "f") {
    event.preventDefault();
    execute("find");
    return;
  }
  if (primary && key === "h") {
    event.preventDefault();
    execute("replace");
    return;
  }
  if (primary && key === "b") {
    event.preventDefault();
    execute("buildProject");
    return;
  }
  if (primary && key === "i") {
    event.preventDefault();
    execute("openErireAI");
    return;
  }
  if (primary && key === "w") {
    event.preventDefault();
    if (state.activePath) closeDoc(state.activePath);
    return;
  }
  if (primary && event.key === ",") {
    event.preventDefault();
    execute("openSettings");
    return;
  }
  if (event.key === "F5" && event.shiftKey) {
    event.preventDefault();
    execute("stopProcess");
    return;
  }
  if (event.key === "F5") {
    event.preventDefault();
    execute("runProject");
    return;
  }
  if (event.key === "F6") {
    event.preventDefault();
    execute("checkProject");
  }
});

if (window.chrome?.webview) {
  window.chrome.webview.addEventListener("message", (event) => onNative(event.data));
}

bridge.post("app:ready");
bridge.post("settings:load");
loadAiSettings();
renderAll();
renderTemplateHint();
appendAiMessage("assistant", "Erire AI is ready inside Studio.");
renderAiPanel();
if (state.bottomTab === "terminal") {
  ui.terminalInput?.focus();
}
