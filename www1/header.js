document.addEventListener('DOMContentLoaded', function () {
  var placeholder = document.getElementById('header-placeholder');
  if (!placeholder) return;
  fetch('/header.html')
    .then(function (res) {
      if (!res.ok) throw new Error('Failed to load header');
      return res.text();
    })
    .then(function (html) {
      placeholder.innerHTML = html;
    })
    .catch(function (err) {
      placeholder.innerHTML = '<div class="muted">Header failed to load</div>';
      console.error(err);
    });
});