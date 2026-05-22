import { defineConfig } from 'vite';
import preact from '@preact/preset-vite';
import { execSync } from 'node:child_process';

// Build-time provenance shown at the bottom of the status page. Recomputed on
// every `vite build` so the bundle on the device matches what we report.
const buildTime = new Date().toISOString();
let gitCommit = 'unknown';
try {
	gitCommit = execSync('git rev-parse --short HEAD', { stdio: ['ignore', 'pipe', 'ignore'] })
		.toString()
		.trim();
	const dirty = execSync('git status --porcelain', { stdio: ['ignore', 'pipe', 'ignore'] })
		.toString()
		.trim().length > 0;
	if (dirty) gitCommit += '-dirty';
} catch {
	// not a git checkout or git missing — leave as "unknown"
}

// https://vitejs.dev/config/
export default defineConfig({
	plugins: [preact()],
	define: {
		__WEBAPP_BUILD_TIME__: JSON.stringify(buildTime),
		__WEBAPP_GIT_COMMIT__: JSON.stringify(gitCommit),
	},
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
