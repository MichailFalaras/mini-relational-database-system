import { BrowserRouter, Routes, Route } from "react-router-dom";
import AppLayout from "./layout/AppLayout.jsx";
import AuthPage from "./pages/AuthPage.jsx";
import HomePage from "./pages/HomePage.jsx";
import ProtectedElement from "./components/ProtectedElement.jsx";

function App() {

  return (
    <BrowserRouter>

      {/* AppLayout is the base element/layout of all pages */}
      <Routes element={<AppLayout />}>
        <Route path="/auth" element={<AuthPage />}/>
        <Route path="/home" element={
          <ProtectedElement>
            <HomePage />
          </ProtectedElement>
          }
        />
      </Routes>
    </BrowserRouter>
  );
}

export default App;