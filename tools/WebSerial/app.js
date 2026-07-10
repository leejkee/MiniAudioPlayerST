/* ============================================================
   MiniAudioPlayerST — WebSerial Monitor
   Pure JS, no frameworks.  Uses the Web Serial API.
   ============================================================ */

(() => {

  /* ---- DOM refs ---- */
  const btnConnect   = document.getElementById('btn-connect');
  const btnClear     = document.getElementById('btn-clear');
  const btnSend      = document.getElementById('btn-send');
  const selBaud      = document.getElementById('baudrate');
  const cbAutoScroll = document.getElementById('autoscroll');
  const cbTimestamps = document.getElementById('timestamps');
  const elOutput     = document.getElementById('output');
  const elInput      = document.getElementById('send-input');
  const elStatus     = document.getElementById('status');
  const elFooter     = document.getElementById('footer-info');

  /* ---- state ---- */
  let port        = null;
  let reader      = null;
  let writer      = null;
  let rxCount     = 0;
  let txCount     = 0;
  let readClosed  = null;    // Promise that resolves when reader cancels

  /* ---- helpers ---- */
  function fmtTS() {
    const now = new Date();
    const pad = (n) => String(n).padStart(2, '0');
    return `[${pad(now.getHours())}:${pad(now.getMinutes())}:${pad(now.getSeconds())}.${String(now.getMilliseconds()).padStart(3, '0')}]`;
  }

  function updateFooter() {
    elFooter.textContent = `Rx: ${rxCount} | Tx: ${txCount}`;
  }

  function appendText(text) {
    if (cbTimestamps.checked) {
      elOutput.textContent += fmtTS() + ' ' + text;
    } else {
      elOutput.textContent += text;
    }
    rxCount += text.length;
    updateFooter();

    if (cbAutoScroll.checked) {
      requestAnimationFrame(() => { elOutput.scrollTop = elOutput.scrollHeight; });
    }
  }

  /* ---- reader loop ---- */
  async function startReading() {
    readClosed = new Promise((resolve) => {
      (async () => {
        try {
          while (port && port.readable) {
            reader = port.readable.getReader();
            try {
              while (true) {
                const { value, done } = await reader.read();
                if (done) break;
                if (value) {
                  const text = new TextDecoder().decode(value);
                  appendText(text);
                }
              }
            } finally {
              reader.releaseLock();
              reader = null;
            }
          }
        } catch (e) {
          // Expected when port is closed
        } finally {
          resolve();
        }
      })();
    });
  }

  /* ---- connect ---- */
  async function connect() {
    try {
      port = await navigator.serial.requestPort();
      await port.open({ baudRate: Number(selBaud.value) });

      writer = port.writable.getWriter();

      elStatus.textContent = 'CONNECTED';
      elStatus.className   = 'status-connected';
      btnConnect.textContent = 'Disconnect';
      btnClear.disabled    = false;
      btnSend.disabled     = false;
      elInput.disabled     = false;
      selBaud.disabled     = true;

      startReading();
    } catch (e) {
      if (e.name !== 'AbortError') {
        appendText(`\r\n[ERROR] ${e.message}\r\n`);
      }
      // User cancelled the port picker — do nothing
    }
  }

  /* ---- disconnect ---- */
  async function disconnect() {
    try {
      if (reader) {
        await reader.cancel();
      }
    } catch (_) { /* ignore */ }

    if (readClosed) {
      await readClosed;
      readClosed = null;
    }

    try {
      if (writer) {
        writer.releaseLock();
        writer = null;
      }
    } catch (_) { /* ignore */ }

    try {
      if (port) {
        await port.close();
      }
    } catch (_) { /* ignore */ }

    port = null;

    elStatus.textContent = 'DISCONNECTED';
    elStatus.className   = 'status-disconnected';
    btnConnect.textContent = 'Connect';
    btnClear.disabled    = true;
    btnSend.disabled     = true;
    elInput.disabled     = true;
    selBaud.disabled     = false;
  }

  /* ---- send ---- */
  async function send(data) {
    if (!writer) return;
    try {
      const enc = new TextEncoder().encode(data);
      await writer.write(enc);
      txCount += enc.length;
      updateFooter();
    } catch (e) {
      appendText(`\r\n[ERROR] Write failed: ${e.message}\r\n`);
    }
  }

  function sendLine() {
    const text = elInput.value;
    if (text.length === 0) return;
    send(text + '\r\n');
    elInput.value = '';
  }

  /* ---- event binding ---- */
  btnConnect.addEventListener('click', () => {
    if (port) {
      disconnect();
    } else {
      connect();
    }
  });

  btnClear.addEventListener('click', () => {
    elOutput.textContent = '';
  });

  btnSend.addEventListener('click', sendLine);

  elInput.addEventListener('keydown', (e) => {
    if (e.key === 'Enter') {
      e.preventDefault();
      sendLine();
    }
  });

  /* ---- graceful cleanup on page unload ---- */
  window.addEventListener('beforeunload', () => {
    if (port) {
      disconnect();
    }
  });

  /* ---- init ---- */
  btnClear.disabled = true;
  btnSend.disabled  = true;
  elInput.disabled  = true;
  updateFooter();

})();
