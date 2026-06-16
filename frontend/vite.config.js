import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";
// Vite dev server proxies /api -> FastAPI bridge on :8000,
// so the frontend can call relative URLs with no CORS surprises.
export default defineConfig({
    plugins: [react()],
    server: {
        port: 5173,
        proxy: {
            "/api": {
                target: "http://localhost:8000",
                changeOrigin: true,
                rewrite: function (path) { return path.replace(/^\/api/, ""); },
            },
        },
    },
});
