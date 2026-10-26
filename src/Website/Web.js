function toggleDarkMode() {
    document.body.classList.toggle('dark-mode');

    const icon = document.getElementById('theme-icon');
  
    if (document.body.classList.contains('dark-mode')) {
        icon.textContent = '☀️';
    } else {
        icon.textContent = '🌙';
    }
}