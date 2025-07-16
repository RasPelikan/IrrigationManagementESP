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
				target: 'https://im.pelikan-it.com:443',
				changeOrigin: true,
				secure: false,
			},
		},
	},
});
