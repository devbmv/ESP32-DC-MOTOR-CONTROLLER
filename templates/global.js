// ============================================================
// 🌍 global.js
// Contains reusable UI helpers for feedback modals, alerts, etc.
// ============================================================

/**
 * Display a Bootstrap modal with a feedback message.
 * @param {string} message - Text to display inside the modal body.
 * @param {boolean} success - Whether the message is a success or an error.
 */
function showModal(message, success = true) {
  const body = document.getElementById('feedbackBody');
  const header = document.getElementById('feedbackLabel');
  const modalEl = document.getElementById('feedbackModal');

  if (header) header.textContent = success ? "✅ Success" : "❌ Error";
  if (body) body.textContent = message || "Operation completed.";

  if (modalEl) {
    // Initialize or reuse modal instance
    const modal = bootstrap.Modal.getOrCreateInstance(modalEl);
    modal.show();

    // Optional: auto-close after 3 seconds
    setTimeout(() => {
      if (modalEl.classList.contains('show')) modal.hide();
    }, 3000);
  } else {
    // Fallback if modal doesn't exist in DOM
    alert((success ? "✅ " : "❌ ") + message);
  }
}

/**
 * Fix Bootstrap's aria-hidden focus warning on modal close.
 * Prevents Chrome accessibility console messages.
 */
document.addEventListener('hidden.bs.modal', e => {
  const active = document.activeElement;
  if (active && active.getAttribute('aria-hidden') === 'true') {
    active.blur(); // clears focus safely
  }
});

/**
 * Play a short sound effect on success/error.
 * You can trigger playSound(true) or playSound(false) anywhere.
 */
function playSound(success = true) {
  const src = success ? "/sounds/success.mp3" : "/sounds/error.mp3";
  try {
    const audio = new Audio(src);
    audio.volume = 0.5;
    audio.play().catch(() => {});
  } catch (err) {
    console.warn("Audio playback failed:", err);
  }
}
