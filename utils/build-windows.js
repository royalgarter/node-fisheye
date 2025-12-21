const { execSync } = require('child_process');
const path = require('path');

function run(cmd) {
    try {
        return execSync(cmd).toString().trim();
    } catch (e) {
        console.error(`Error running: ${cmd}`);
        process.exit(1);
    }
}

console.log('Fetching OpenCV configuration...');
const findOpenCV = path.join(__dirname, 'find-opencv.js');

// Get Includes
// Output format from find-opencv.js: "path1"\n"path2"
const cflagsRaw = run(`node "${findOpenCV}" --cflags`);
const includes = cflagsRaw.split(/\r?\n/)
    .filter(Boolean)
    .map(p => `/I${p}`) // Prepend /I for MSVC
    .join(' ');

// Get Libs
// Output format: "path/to/lib1.lib" ...
const libsRaw = run(`node "${findOpenCV}" --libs`);
// find-opencv.js outputs paths wrapped in quotes, which is good.
// We just need to join them with spaces if they aren't already valid.
// The output seems to be space/newline separated strings.
const libs = libsRaw.replace(/\r?\n/g, ' ');

const src = path.join('src', 'cli.cc');
const out = 'fisheye.exe';

// Compile Command for MSVC (cl.exe)
// /EHsc: Enable C++ exceptions
// /std:c++17: C++17 Standard
// /Fe: Output filename
// /Od: Disable optimization (debug) - optional, remove for release /O2
const compileCmd = `cl /EHsc /std:c++17 /O2 "${src}" /Fe:"${out}" ${includes} ${libs}`;

console.log('Compiling...');
console.log(compileCmd);

try {
    execSync(compileCmd, { stdio: 'inherit' });
    console.log(`\nSuccess! Created ${out}`);
} catch (e) {
    console.error('Compilation failed.');
    process.exit(1);
}

