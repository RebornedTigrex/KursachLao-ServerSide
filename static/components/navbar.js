class CustomNavbar extends HTMLElement {
  connectedCallback() {
    this.attachShadow({ mode: 'open' });
    this.shadowRoot.innerHTML = `
      <style>
        :host {
          display: block;
          width: 100%;
          background-color: white;
          box-shadow: 0 1px 3px rgba(0, 0, 0, 0.1);
          position: sticky;
          top: 0;
          z-index: 50;
          padding-left: 1rem;
          padding-right: 1rem;
        }
.navbar-container {
          max-width: 1280px;
          margin: 0 auto;
          padding: 0.75rem 1.5rem;
          display: flex;
          align-items: center;
          justify-content: space-between;
        }
        
        .logo-link {
          display: flex;
          align-items: center;
          text-decoration: none;
          color: #1f2937;
          font-weight: 600;
          font-size: 1.25rem;
          gap: 0.5rem;
          padding: 0.5rem 1rem;
          border-radius: 0.5rem;
          transition: all 0.2s ease;
        }
        
        .logo-link:hover {
          background-color: #f3f4f6;
          color: #3b82f6;
        }
        
        .logo-icon {
          width: 1.5rem;
          height: 1.5rem;
          color: #3b82f6;
        }

        .nav-buttons {
          display: flex;
          gap: 0.5rem;
        }

        .nav-button {
          display: flex;
          align-items: center;
          gap: 0.5rem;
          padding: 0.5rem 1rem;
          border-radius: 9999px;
          background-color: #f3f4f6;
          color: #1f2937;
          text-decoration: none;
          font-size: 0.875rem;
          font-weight: 500;
          transition: all 0.2s ease;
        }

        .nav-button:hover {
          background-color: #e5e7eb;
        }

        .nav-button i {
          width: 1rem;
          height: 1rem;
        }
      </style>
      
      <div class="navbar-container">
        <a href="index.html" class="logo-link">
          <svg class="logo-icon" xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
            <path d="M21 11.5a8.38 8.38 0 0 1-.9 3.8 8.5 8.5 0 0 1-7.6 4.7 8.38 8.38 0 0 1-3.8-.9L3 21l1.9-5.7a8.38 8.38 0 0 1-.9-3.8 8.5 8.5 0 0 1 4.7-7.6 8.38 8.38 0 0 1 3.8-.9h.5a8.48 8.48 0 0 1 8 8v.5z"></path>
          </svg>
          <span>HR Harmony Hub</span>
        </a>
        <div class="nav-buttons">
          <a href="index.html" class="nav-button">
            <span>Dashboard</span>
          </a>
          <a href="employees.html" class="nav-button">
            <span>Employees</span>
          </a>
          <a href="salary.html" class="nav-button">
            <span>Salary</span>
          </a>
          <a href="hours.html" class="nav-button">
            <span>Hours</span>
          </a>
        </div>
</div>
`;
  }
}
customElements.define('custom-navbar', CustomNavbar);

// Replace feather icons after navbar is rendered
document.addEventListener('DOMContentLoaded', () => {
  if (window.feather) {
    feather.replace();
  }
});
