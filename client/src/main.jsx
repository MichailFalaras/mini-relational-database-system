import { StrictMode } from 'react'
import { createRoot } from 'react-dom/client'
import './index.css'
import App from './App.jsx'
import { AuthProvider } from "./context/AuthProvider.jsx";


createRoot(document.getElementById('root')).render(
  <StrictMode>
    {/* Enclosing the whole App in the AuthProvider context,
      in order to be able to access user auth information */}
    <AuthProvider>
      <App />
    </AuthProvider>
  </StrictMode>,
)
