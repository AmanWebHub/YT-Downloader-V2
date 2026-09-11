(function () {
  const BTN_ID = "ytdlp-download-btn";
  let lastUrl = "";

  function isWatchPage() {
    return /^https:\/\/www\.youtube\.com\/(watch|shorts\/)/.test(location.href);
  }

  function addButton() {
    if (document.getElementById(BTN_ID)) return;

    const btn = document.createElement("button");
    btn.id = BTN_ID;
    btn.textContent = "\u2B07 Download";
    btn.style.cssText = [
      "position:fixed",
      "bottom:20px",
      "right:20px",
      "z-index:999999",
      "background:#e02020",
      "color:#fff",
      "border:none",
      "padding:10px 18px",
      "border-radius:8px",
      "font-size:14px",
      "font-family:Arial,Helvetica,sans-serif",
      "font-weight:bold",
      "cursor:pointer",
      "box-shadow:0 2px 10px rgba(0,0,0,0.35)",
    ].join(";");

    btn.addEventListener("click", () => {
      const videoUrl = window.location.href;
      const link = "ytdlp://" + encodeURIComponent(videoUrl);
      window.location.href = link;
    });

    document.body.appendChild(btn);
  }

  function removeButton() {
    const existing = document.getElementById(BTN_ID);
    if (existing) existing.remove();
  }

  function syncButton() {
    if (isWatchPage()) {
      addButton();
    } else {
      removeButton();
    }
  }

  // Initial check (covers direct loads and full reloads)
  syncButton();

  // YouTube fires this custom event on its document when a client-side
  // ("SPA") navigation finishes - e.g. clicking from the homepage to a
  // video without a full page reload. This is the fast, reliable signal.
  document.addEventListener("yt-navigate-finish", syncButton);

  // Fallback: some navigations (or older YouTube builds) may not fire
  // that event, so also poll the URL periodically as a safety net.
  setInterval(() => {
    if (location.href !== lastUrl) {
      lastUrl = location.href;
      syncButton();
    }
  }, 750);

  // Also reattach the button if YouTube's own re-renders happen to wipe
  // it out while staying on the same watch page.
  const observer = new MutationObserver(() => {
    if (isWatchPage()) addButton();
  });
  observer.observe(document.body, { childList: true, subtree: true });
})();