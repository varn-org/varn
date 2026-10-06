import tailwindcss from "@tailwindcss/vite";
import { defineConfig } from "vite";

export default defineConfig({
  plugins: [tailwindcss()],
  worker: {
    format: "es",
  },
  server: {
    port: 5174,
  },
});
