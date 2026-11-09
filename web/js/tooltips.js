// Help tooltips for section headers. Any element with data-tip="..." gets a small
// info icon. The text shows on hover over the header, on keyboard focus of the icon,
// and on tap of the icon (touch). A single fixed-position tooltip is used, so
// scrolling containers such as the sidebar can't clip it.

export function initTooltips(root = document) {
  const tip = document.createElement('div');
  tip.id = 'hoverTip';
  tip.className = 'hover-tip';
  tip.setAttribute('role', 'tooltip');
  tip.hidden = true;
  document.body.append(tip);

  let current = null; // header whose tip is showing

  const place = (icon) => {
    const r = icon.getBoundingClientRect();
    const margin = 8;
    tip.style.left = '0px';
    tip.style.top = '0px';
    const { width, height } = tip.getBoundingClientRect();
    const left = Math.min(Math.max(margin, r.left - 12), window.innerWidth - width - margin);
    let top = r.bottom + 8;
    if (top + height > window.innerHeight - margin) top = Math.max(margin, r.top - height - 8);
    tip.style.left = `${left}px`;
    tip.style.top = `${top}px`;
  };

  const show = (header, icon) => {
    current = header;
    tip.textContent = header.dataset.tip;
    tip.hidden = false;
    icon.setAttribute('aria-describedby', tip.id);
    place(icon);
  };

  const hide = () => {
    if (!current) return;
    current.querySelector('.info')?.removeAttribute('aria-describedby');
    current = null;
    tip.hidden = true;
  };

  for (const header of root.querySelectorAll('[data-tip]')) {
    const label = header.firstChild?.textContent?.trim() || 'this section';
    const icon = document.createElement('button');
    icon.type = 'button';
    icon.className = 'info';
    icon.textContent = 'i';
    icon.setAttribute('aria-label', `About ${label}`);
    // Put the icon right after the header's own label, before any sub-caption.
    header.insertBefore(icon, header.querySelector('.sub'));

    header.addEventListener('mouseenter', () => show(header, icon));
    header.addEventListener('mouseleave', hide);
    icon.addEventListener('focus', () => show(header, icon));
    icon.addEventListener('blur', hide);
    icon.addEventListener('click', (e) => {
      e.preventDefault(); // inside <summary>: don't collapse the section
      e.stopPropagation();
      if (current === header) hide(); else show(header, icon);
    });
  }

  // Dismiss on Esc, on a tap elsewhere, and when anything scrolls (the anchor moves).
  document.addEventListener('keydown', (e) => { if (e.key === 'Escape') hide(); });
  document.addEventListener('pointerdown', (e) => {
    if (current && !current.contains(e.target)) hide();
  });
  window.addEventListener('scroll', hide, true);
  window.addEventListener('resize', hide);
}
