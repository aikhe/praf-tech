#!/usr/bin/env node

const { execSync } = require('child_process');

// Force npm installation instead of bun
console.log('🚀 Installing dependencies with npm...');
try {
  execSync('npm install', { stdio: 'inherit' });
  console.log('✅ Dependencies installed successfully');
  
  console.log('🏗️ Building the project...');
  execSync('npm run build', { stdio: 'inherit' });
  console.log('✅ Build completed successfully');
} catch (error) {
  console.error('❌ Build failed:', error);
  process.exit(1);
} 