// 用真实浏览器回归设备管理网页：mock 设备 + Chrome DevTools Protocol。
// 需要本机有 Chrome 或 Edge；没有则跳过。不参与固件构建。
//   node tools/device_admin/ui_check.cjs
const { spawn } = require('node:child_process');
const fs = require('node:fs');
const path = require('node:path');

const ROOT = path.resolve(__dirname, '../..');
const OUT = path.join(ROOT, 'tmp/device_admin_ui');
const SHOTS = path.join(OUT, 'shots');
const MOCK_PORT = Number(process.env.MOCK_PORT || 8123);
const DEVTOOLS = Number(process.env.DEVTOOLS_PORT || 9333);
const MOCK = 'http://127.0.0.1:' + MOCK_PORT + '/';

const CANDIDATES = [
  process.env.CHROME,
  'C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe',
  'C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe',
  'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe',
  'C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe',
  '/usr/bin/google-chrome',
  '/usr/bin/chromium'
].filter(Boolean);
const CHROME = CANDIDATES.find((p) => fs.existsSync(p));

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const results = [];
function check(name, ok, detail) {
  results.push({ name, ok });
  console.log((ok ? 'PASS ' : 'FAIL ') + name + (detail ? '  ' + detail : ''));
}

class CDP {
  constructor(ws) {
    this.ws = ws;
    this.id = 0;
    this.pending = new Map();
    this.logs = [];
    ws.onmessage = (ev) => {
      const msg = JSON.parse(ev.data);
      if (msg.id && this.pending.has(msg.id)) {
        const { resolve, reject } = this.pending.get(msg.id);
        this.pending.delete(msg.id);
        msg.error ? reject(new Error(msg.method + ': ' + msg.error.message)) : resolve(msg.result);
        return;
      }
      if (msg.method === 'Runtime.exceptionThrown') {
        const d = msg.params.exceptionDetails;
        this.logs.push('EXCEPTION ' + (d.text || '') + ' ' + (d.exception ? d.exception.description : ''));
      }
      if (msg.method === 'Runtime.consoleAPICalled' && msg.params.type === 'error') {
        this.logs.push('CONSOLE ' + msg.params.args.map((a) => a.value || a.description).join(' '));
      }
      if (msg.method === 'Log.entryAdded' && msg.params.entry.level === 'error') {
        this.logs.push('LOG ' + msg.params.entry.text);
      }
    };
  }
  send(method, params = {}) {
    const id = ++this.id;
    this.ws.send(JSON.stringify({ id, method, params }));
    return new Promise((resolve, reject) => this.pending.set(id, { resolve, reject }));
  }
}

async function waitForUrl(url, timeout = 20000) {
  const end = Date.now() + timeout;
  while (Date.now() < end) {
    try { await fetch(url); return true; } catch (e) { await sleep(150); }
  }
  return false;
}

async function main() {
  if (!CHROME) {
    console.log('SKIP: 未找到 Chrome/Edge，可设置环境变量 CHROME 指向浏览器可执行文件');
    return;
  }
  fs.rmSync(OUT, { recursive: true, force: true });
  fs.mkdirSync(SHOTS, { recursive: true });

  const mock = spawn(process.execPath, [path.join(__dirname, 'mock_server.cjs'), String(MOCK_PORT)], { stdio: 'ignore' });
  const chrome = spawn(CHROME, [
    '--headless=new', '--disable-gpu', '--hide-scrollbars', '--no-first-run',
    '--no-default-browser-check', '--user-data-dir=' + path.join(OUT, 'profile'),
    '--remote-debugging-port=' + DEVTOOLS, 'about:blank'
  ], { stdio: 'ignore' });
  let ws = null;

  try {
    if (!await waitForUrl(MOCK + 'api/overview')) throw new Error('mock 服务没有就绪（端口 ' + MOCK_PORT + ' 可能被占用）');
    await fetch(MOCK + '__reset');
    const devtools = 'http://127.0.0.1:' + DEVTOOLS;

    let version = null;
    for (let i = 0; i < 80; i++) {
      try { version = await (await fetch(devtools + '/json/version')).json(); break; }
      catch (e) { await sleep(200); }
    }
    if (!version) throw new Error('浏览器 DevTools 端口没有就绪');

    const target = await (await fetch(devtools + '/json/new?' + encodeURIComponent(MOCK), { method: 'PUT' })).json();
    ws = new WebSocket(target.webSocketDebuggerUrl);
    await new Promise((r, j) => { ws.onopen = r; ws.onerror = j; });
    const cdp = new CDP(ws);
    await cdp.send('Page.enable');
    await cdp.send('Runtime.enable');
    await cdp.send('Log.enable');

    const metrics = (height) => cdp.send('Emulation.setDeviceMetricsOverride', {
      width: 390, height: height || 844, deviceScaleFactor: 2, mobile: true
    });
    await metrics();
    await cdp.send('Emulation.setTouchEmulationEnabled', { enabled: true, maxTouchPoints: 5 });

    const run = async (expr) => {
      const r = await cdp.send('Runtime.evaluate', { expression: expr, awaitPromise: true, returnByValue: true });
      if (r.exceptionDetails) throw new Error('evaluate 失败: ' + JSON.stringify(r.exceptionDetails));
      return r.result.value;
    };
    const shot = async (name, height) => {
      if (height) await metrics(height);
      const { data } = await cdp.send('Page.captureScreenshot', { format: 'png' });
      fs.writeFileSync(path.join(SHOTS, name + '.png'), Buffer.from(data, 'base64'));
      if (height) await metrics();
    };
    const waitFor = async (expr, label, timeout = 8000) => {
      const end = Date.now() + timeout;
      while (Date.now() < end) {
        if (await run('Boolean(' + expr + ')')) return true;
        await sleep(80);
      }
      check('等待超时: ' + label, false);
      return false;
    };

    await cdp.send('Page.navigate', { url: MOCK });
    await waitFor("document.readyState === 'complete'", '页面加载');
    await sleep(500);
    check('登录页显示', await run("!document.getElementById('auth').hidden"));
    await shot('01-login');

    await run("document.getElementById('code').value='000000';document.getElementById('login').click();");
    await sleep(400);
    check('错误验证码提示', await run("!document.getElementById('loginError').hidden"));
    await shot('02-login-error');

    await run("document.getElementById('code').value='123456';document.getElementById('login').click();");
    await waitFor("!document.getElementById('shell').hidden", '登录完成');
    await sleep(600);
    check('概览渲染四个状态卡', (await run("document.querySelectorAll('#content .tile').length")) === 4);
    await shot('03-overview');

    await run("document.querySelector('[data-tab=files]').click()");
    await waitFor("document.querySelector('#content .list, #content .empty')", '文件列表');
    await sleep(400);
    check('根目录 7 项', (await run("document.querySelectorAll('#content .item').length")) === 7);
    await shot('04-files-list');

    await run("document.getElementById('vGrid').click()");
    await sleep(250);
    check('网格视图', await run("!!document.querySelector('#content .list.grid')"));
    await shot('05-files-grid');

    await run("FS.view='list'; FS.picked=new Set(['/Music','/Book']); enterSel();");
    await sleep(250);
    check('多选头部计数', (await run("document.querySelector('#top .titlebox b').textContent")) === '已选 2 项');
    check('多选时删除可用、下载禁用', !(await run("document.getElementById('aDelete').disabled")) &&
      (await run("document.getElementById('aDownload').disabled")));
    await shot('06-files-select');

    await run("exitSel(); sheetFile('/Music/夜曲.mp3');");
    await sleep(300);
    check('文件操作弹层', await run("!!document.getElementById('fDown')"));
    await shot('07-file-sheet');

    await run("closeSheet(); sheetNew();");
    await sleep(300);
    check('新建/上传弹层', await run("!!document.getElementById('mUpload')"));
    await shot('08-new-sheet');

    await run(`closeSheet();
      const dt=new DataTransfer();
      dt.items.add(new File([new Uint8Array(400000)],'新歌.mp3'));
      const inp=document.getElementById('file');
      inp.files=dt.files;
      inp.dispatchEvent(new Event('change'));`);
    await sleep(500);
    check('上传进度弹层', await run("!!document.getElementById('upbar')"));
    await shot('09-upload');
    await waitFor("uploadBusy === false", '上传结束', 10000);
    await sleep(400);
    check('上传后列表出现新文件', await run("FS.rows.some(r=>r.name==='新歌.mp3')"));

    await run("openDir('/models')");
    await sleep(500);
    check('空目录提示', await run("!!document.querySelector('#content .empty')"));
    await shot('10-empty');

    await run("document.querySelector('[data-tab=settings]').click()");
    await waitFor("document.getElementById('save')", '设置页');
    await sleep(300);
    check('设置页回填名称', (await run("document.getElementById('name').value")).length > 0);
    // 设备名称按 UTF-8 字节校验（设备端 name[33]）：超长必须在前端拦下，不做无效请求。
    await run("document.getElementById('name').value='一二三四五六七八九十一二'; document.getElementById('save').click();");
    await sleep(500);
    check('超长名称被前端拦下', (await run("document.getElementById('toast').textContent")).includes('32 字节'));
    check('超长名称未提交', (await run("SYS.name")) === 'OpenBread N16R8');
    await run("document.getElementById('name').value='我的阅读器'; document.getElementById('save').click();");
    await sleep(700);
    check('合法名称保存成功', (await run("SYS.name")) === '我的阅读器');
    await shot('11-settings');

    await run("document.querySelector('[data-tab=update]').click()");
    await waitFor("document.getElementById('check')", '更新页');
    await sleep(300);
    await run("document.getElementById('check').click()");
    await sleep(600);
    check('在线更新状态刷新', (await run("document.getElementById('status').textContent")).includes('发现新版本'));
    await shot('12-update', 1100);

    await run("document.querySelector('[data-tab=files]').click()");
    await sleep(500);
    await run("document.getElementById('fab').click()");
    await sleep(300);
    await run("document.getElementById('mMkdir').click()");
    await sleep(300);
    await run("document.getElementById('pName').value='新目录'; document.getElementById('pOk').click();");
    await sleep(800);
    check('新建文件夹生效', await run("FS.rows.some(r=>r.name==='新目录')"));

    await run("FS.picked=new Set(['/新目录']); enterSel();");
    await sleep(200);
    await run("document.getElementById('aRename').click()");
    await sleep(300);
    await run("document.getElementById('pName').value='改名后'; document.getElementById('pOk').click();");
    await sleep(800);
    check('重命名生效', await run("FS.rows.some(r=>r.name==='改名后') && !FS.rows.some(r=>r.name==='新目录')"));

    await run("FS.picked=new Set(['/改名后']); enterSel();");
    await sleep(200);
    await run("document.getElementById('aDelete').click()");
    await sleep(300);
    check('删除二次确认', await run("!!document.getElementById('cOk')"));
    await run("document.getElementById('cOk').click();");
    await sleep(900);
    check('删除生效', !(await run("FS.rows.some(r=>r.name==='改名后')")));

    await run("FS.picked=new Set(['/Music']); enterSel();");
    await sleep(200);
    check('文件夹禁止下载', await run("document.getElementById('aDownload').disabled"));
    await run("exitSel();");

    await run("token='bad-token'; show('overview');");
    await sleep(800);
    check('令牌失效回到登录页', await run("!document.getElementById('auth').hidden"));

    const bad = cdp.logs.filter((l) => !l.includes('favicon') && !l.includes('Failed to load resource'));
    check('无脚本异常与控制台错误', bad.length === 0, bad.slice(0, 3).join(' | '));
  } finally {
    if (ws) ws.close();
    chrome.kill();
    mock.kill();
  }

  const failed = results.filter((r) => !r.ok);
  console.log('\n截图: ' + SHOTS);
  console.log(failed.length ? failed.length + ' / ' + results.length + ' 项失败' : '全部 ' + results.length + ' 项通过');
  process.exitCode = failed.length ? 1 : 0;
}

main().catch((e) => { console.error(e); process.exitCode = 1; });
