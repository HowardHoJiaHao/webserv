document.addEventListener('DOMContentLoaded', function () {
  var placeholder = document.getElementById('header-placeholder');
  if (!placeholder) return;

  function normalizePath(path) {
    if (!path) return '/';
    var clean = path.split('?')[0].split('#')[0];
    if (clean === '') return '/';
    if (clean.length > 1 && clean.endsWith('/')) clean = clean.slice(0, -1);
    return clean;
  }

  function markActiveLink(container) {
    var currentPath = normalizePath(window.location.pathname);
    if (currentPath === '/') currentPath = '/index.html';

    var links = container.querySelectorAll('nav a[href]');
    links.forEach(function (link) {
      var href = link.getAttribute('href');
      if (!href) return;

      var linkPath = normalizePath(href);
      var isActive = currentPath === linkPath;

      if (isActive) {
        link.classList.add('nav-active');
        link.setAttribute('aria-current', 'page');
      }
    });
  }

  fetch('/header.html')
    .then(function (res) {
      if (!res.ok) throw new Error('Failed to load header');
      return res.text();
    })
    .then(function (html) {
      placeholder.innerHTML = html;
      markActiveLink(placeholder);
    })
    .catch(function (err) {
      placeholder.innerHTML = '<div class="muted">Header failed to load</div>';
      console.error(err);
    });
});