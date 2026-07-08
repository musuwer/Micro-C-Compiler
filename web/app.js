const DATA_ROOT = "data";

const els = {
  reload: document.querySelector("#reloadBtn"),
  expandAst: document.querySelector("#expandAstBtn"),
  collapseAst: document.querySelector("#collapseAstBtn"),
  copySource: document.querySelector("#copySourceBtn"),
  demoSelect: document.querySelector("#demoSelect"),
  demoNote: document.querySelector("#demoNote"),
  tokenSearch: document.querySelector("#tokenSearch"),
  source: document.querySelector("#sourceView"),
  sourceMeta: document.querySelector("#sourceMeta"),
  tokenRows: document.querySelector("#tokenRows"),
  astTree: document.querySelector("#astTree"),
  symbolRows: document.querySelector("#symbolRows"),
  tac: document.querySelector("#tacView"),
  bytecode: document.querySelector("#bytecodeView"),
  vmOutput: document.querySelector("#vmOutputView"),
  diagnostics: document.querySelector("#diagnosticsView"),
  tokenCount: document.querySelector("#tokenCount"),
  astCount: document.querySelector("#astCount"),
  symbolCount: document.querySelector("#symbolCount"),
  vmResult: document.querySelector("#vmResult")
};

let sourceLines = [];
let activeRange = null;
let tokenCache = [];
let currentSource = "";

async function loadJson(path, fallback) {
  try {
    const res = await fetch(path, { cache: "no-store" });
    if (!res.ok) throw new Error(`${res.status} ${res.statusText}`);
    return await res.json();
  } catch (err) {
    console.warn(`Cannot load ${path}:`, err);
    return fallback;
  }
}

async function loadText(path, fallback = "") {
  try {
    const res = await fetch(path, { cache: "no-store" });
    if (!res.ok) throw new Error(`${res.status} ${res.statusText}`);
    return await res.text();
  } catch (err) {
    console.warn(`Cannot load ${path}:`, err);
    return fallback;
  }
}

function text(value) {
  return value === undefined || value === null ? "" : String(value);
}

function escapeHtml(value) {
  return String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#39;");
}

function renderSourceLines() {
  els.source.innerHTML = sourceLines.map((line, index) => {
    const lineNo = index + 1;
    let lineHtml = escapeHtml(line || " ");

    if (activeRange && activeRange.line === lineNo && activeRange.col > 0 && activeRange.length > 0) {
      const start = Math.max(0, activeRange.col - 1);
      const end = Math.min(line.length, start + activeRange.length);
      const before = escapeHtml(line.slice(0, start));
      const token = escapeHtml(line.slice(start, end) || " ");
      const after = escapeHtml(line.slice(end));
      lineHtml = `${before}<mark class="token-mark">${token}</mark>${after || ""}`;
    }

    const activeClass = activeRange && activeRange.line === lineNo ? " is-active" : "";
    return `<span class="source-line${activeClass}" data-line="${lineNo}"><span class="line-no">${lineNo}</span><span class="line-text">${lineHtml}</span></span>`;
  }).join("");
}

function renderSource(source) {
  currentSource = source;
  sourceLines = source.split(/\r?\n/);
  activeRange = null;
  renderSourceLines();
}

function highlightLine(line, col = 0, length = 0) {
  document.querySelectorAll("tr.is-active").forEach(el => el.classList.remove("is-active"));
  activeRange = { line: Number(line), col: Number(col) || 0, length: Number(length) || 0 };
  renderSourceLines();
  const target = document.querySelector(`.source-line[data-line="${line}"]`);
  if (!target) return;
  target.scrollIntoView({ block: "center", behavior: "smooth" });
}

function tokenMatches(token, keyword) {
  if (!keyword) return true;
  const haystack = `${token.kind ?? ""} ${token.lexeme ?? ""}`.toLowerCase();
  return haystack.includes(keyword.toLowerCase());
}

function renderTokens(tokens) {
  const keyword = els.tokenSearch.value.trim();
  const rows = tokens
    .map((token, index) => ({ token, index }))
    .filter(({ token }) => tokenMatches(token, keyword));

  els.tokenRows.innerHTML = rows.map(({ token, index }) => `
    <tr data-index="${index}" data-line="${escapeHtml(text(token.line))}" data-col="${escapeHtml(text(token.col))}" data-length="${escapeHtml(text(token.length))}">
      <td>${index}</td>
      <td><span class="pill">${escapeHtml(text(token.kind))}</span></td>
      <td><code>${escapeHtml(text(token.lexeme))}</code></td>
      <td>${escapeHtml(text(token.line))}</td>
      <td>${escapeHtml(text(token.col))}</td>
      <td>${escapeHtml(text(token.length))}</td>
    </tr>
  `).join("");

  els.tokenRows.querySelectorAll("tr").forEach(row => {
    row.addEventListener("click", () => {
      highlightLine(Number(row.dataset.line), Number(row.dataset.col), Number(row.dataset.length));
      row.classList.add("is-active");
    });
  });
}

function renderSymbols(symbols) {
  els.symbolRows.innerHTML = symbols.map(symbol => {
    const scope = symbol.scopeDepth ?? symbol.scope_depth ?? "";
    const declLine = symbol.declLine ?? symbol.decl_line ?? "";
    return `
      <tr data-line="${escapeHtml(text(declLine))}">
        <td><code>${escapeHtml(text(symbol.name))}</code></td>
        <td><span class="pill muted">${escapeHtml(text(symbol.type))}</span></td>
        <td>${escapeHtml(text(scope))}</td>
        <td>${escapeHtml(text(declLine))}</td>
        <td>${symbol.initialized ? "yes" : "no"}</td>
        <td>${symbol.used ? "yes" : "no"}</td>
        <td>${escapeHtml(text(symbol.slot))}</td>
      </tr>
    `;
  }).join("");

  els.symbolRows.querySelectorAll("tr").forEach(row => {
    row.addEventListener("click", () => {
      highlightLine(Number(row.dataset.line));
      row.classList.add("is-active");
    });
  });
}

function childNodes(node) {
  if (!node || typeof node !== "object") return [];
  const result = [];
  if (Array.isArray(node.children)) result.push(...node.children.filter(Boolean));
  ["lhs", "rhs", "condition", "then", "else", "init", "step", "body"].forEach(key => {
    if (node[key]) result.push(node[key]);
  });
  return result;
}

function astNodeCount(node) {
  if (!node || !node.kind) return 0;
  return 1 + childNodes(node).reduce((sum, child) => sum + astNodeCount(child), 0);
}

function renderAstNode(node) {
  if (!node) return document.createTextNode("");
  const children = childNodes(node);
  const details = document.createElement("details");
  details.open = children.length > 0;
  details.className = "ast-node";

  const summary = document.createElement("summary");
  const button = document.createElement("button");
  button.type = "button";
  button.className = "ast-label";
  button.dataset.line = node.line || "";

  const chips = [];
  if (node.id) chips.push(`#${escapeHtml(text(node.id))}`);
  if (node.line) chips.push(`line ${escapeHtml(text(node.line))}`);
  if (node.name) chips.push(`name=${escapeHtml(node.name)}`);
  if (node.op) chips.push(`op=${escapeHtml(node.op)}`);
  if (node.type) chips.push(`type=${escapeHtml(node.type)}`);
  if (node.slot !== undefined) chips.push(`slot=${escapeHtml(text(node.slot))}`);

  button.innerHTML = `
    <span class="ast-kind">${escapeHtml(text(node.kind))}</span>
    ${chips.map(chip => `<span class="ast-meta">${chip}</span>`).join("")}
  `;
  button.addEventListener("click", event => {
    event.stopPropagation();
    document.querySelectorAll(".ast-label.is-active").forEach(el => el.classList.remove("is-active"));
    button.classList.add("is-active");
    if (node.line) highlightLine(node.line);
  });
  summary.appendChild(button);
  details.appendChild(summary);

  const list = document.createElement("div");
  list.className = "ast-children";
  children.forEach(child => list.appendChild(renderAstNode(child)));
  details.appendChild(list);
  return details;
}

function renderAst(ast) {
  els.astTree.innerHTML = "";
  if (!ast || !ast.kind) {
    els.astTree.textContent = "AST data not available.";
    return;
  }
  els.astTree.appendChild(renderAstNode(ast));
}

function setAstOpen(open) {
  els.astTree.querySelectorAll("details").forEach(details => {
    details.open = open;
  });
}

function extractVmResult(output) {
  const match = output.match(/Program return value:\s*(-?\d+)/);
  return match ? match[1] : "-";
}

function demoDataDir() {
  const selected = els.demoSelect.value;
  return selected ? `${DATA_ROOT}/demos/${selected}` : DATA_ROOT;
}

async function loadExampleList() {
  const examples = await loadJson(`${DATA_ROOT}/examples.json`, []);
  if (!Array.isArray(examples) || examples.length === 0) return;

  els.demoSelect.innerHTML = examples.map(example => {
    const label = `${example.title || example.id} · 返回 ${example.expected}`;
    return `<option value="${escapeHtml(example.id)}">${escapeHtml(label)}</option>`;
  }).join("");

  const preferred = examples.find(example => example.id === "ast_complex_demo") || examples[0];
  els.demoSelect.value = preferred.id;
}

async function loadDemo() {
  const base = demoDataDir();
  const [manifest, source, tokens, ast, symbols, tac, bytecode, vmOutput, diagnostics] = await Promise.all([
    loadJson(`${base}/manifest.json`, null),
    loadText(`${base}/source.mc`, ""),
    loadJson(`${base}/tokens.json`, []),
    loadJson(`${base}/ast.json`, null),
    loadJson(`${base}/symbols.json`, []),
    loadText(`${base}/out.tac`, ""),
    loadText(`${base}/out.bc`, ""),
    loadText(`${base}/vm_output.txt`, ""),
    loadText(`${DATA_ROOT}/diagnostics/parse_recovery_demo.txt`, "")
  ]);

  tokenCache = Array.isArray(tokens) ? tokens : [];
  renderSource(source);
  renderTokens(tokenCache);
  renderAst(ast);
  renderSymbols(Array.isArray(symbols) ? symbols : []);
  els.tac.textContent = tac || "TAC data not available.";
  els.bytecode.textContent = bytecode || "Bytecode data not available.";
  els.vmOutput.textContent = vmOutput || "VM output not available.";
  els.diagnostics.textContent = diagnostics || "Diagnostics data not available. Run bash scripts/build_web_demo.sh first.";

  els.tokenCount.textContent = tokenCache.length;
  els.astCount.textContent = astNodeCount(ast);
  els.symbolCount.textContent = Array.isArray(symbols) ? symbols.length : 0;
  els.vmResult.textContent = extractVmResult(vmOutput);
  const sourceLabel = manifest?.source || `${base}/source.mc`;
  els.sourceMeta.textContent = sourceLabel;
  els.demoNote.textContent = `当前数据来源：${sourceLabel}；Web 前端读取 ${base}/ 下的 tokens.json、ast.json、symbols.json、out.tac、out.bc。`;
}

els.reload.addEventListener("click", loadDemo);
els.expandAst.addEventListener("click", () => setAstOpen(true));
els.collapseAst.addEventListener("click", () => setAstOpen(false));
els.demoSelect.addEventListener("change", loadDemo);
els.tokenSearch.addEventListener("input", () => renderTokens(tokenCache));
els.copySource.addEventListener("click", async () => {
  try {
    await navigator.clipboard.writeText(currentSource);
    els.copySource.classList.add("copied");
    setTimeout(() => els.copySource.classList.remove("copied"), 900);
  } catch (err) {
    console.warn("Clipboard copy failed:", err);
  }
});

(async function init() {
  await loadExampleList();
  await loadDemo();
})();
