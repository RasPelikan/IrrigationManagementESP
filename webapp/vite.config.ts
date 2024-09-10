import { defineConfig } from 'vite';
import preact from '@preact/preset-vite';

// https://vitejs.dev/config/
export default defineConfig({
	plugins: [preact()],
	build: {
		outDir: '../data/www',
		emptyOutDir: true
	},
	server: {
		proxy: {
			'/api': {
				target: 'http://10.0.0.35',
				changeOrigin: true,
				secure: false,
			},
		},
	},
});
