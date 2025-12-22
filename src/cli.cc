#include <opencv2/core.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include <iostream>
#include <vector>
#include <filesystem>
#include <string>
#include <algorithm>
#include <cctype>

#include <windows.h> // For Windows GUI
#include <commctrl.h> // For common controls like edit boxes

// Link with libraries
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "Comctl32.lib") // For InitCommonControlsEx

namespace fs = std::filesystem;

// --- WINDOWS GUI GLOBALS AND CONSTANTS ---
// Control IDs
#define IDC_SRC_PATH_EDIT 101
#define IDC_DEST_PATH_EDIT 102
#define IDC_SAMPLES_DIR_EDIT 103
#define IDC_CHECKBOARD_WIDTH_EDIT 104
#define IDC_CHECKBOARD_HEIGHT_EDIT 105
#define IDC_START_BUTTON 106

// Global handles for controls and data storage
HWND hSrcPathEdit, hDestPathEdit, hSamplesDirEdit, hWidthEdit, hHeightEdit;
std::string g_srcPath, g_destPath, g_samplesDir;
int g_checkboardWidth, g_checkboardHeight;
bool g_guiSubmitted = false;
bool g_guiCancelled = false;

// Forward declaration of WndProc
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

// Function to run the Windows GUI
bool runWinGuiMode(std::string& src, std::string& dest, std::string& samples, int& w, int& h) {
    // To ensure common controls are available
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    const char CLASS_NAME[] = "FisheyeCalibratorWindowClass";

    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); // Set background color

    if (!RegisterClassEx(&wc)) {
        MessageBox(nullptr, "Window Registration Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
        return false;
    }

    HWND hwnd = CreateWindowEx(
        0,                              // Optional window style
        CLASS_NAME,                     // Window class
        "Fisheye Calibrator Input",     // Window text
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, // Window style
        CW_USEDEFAULT, CW_USEDEFAULT,   // Position
        660, 420,                       // Size (width, height)
        nullptr,                        // Parent window
        nullptr,                        // Menu
        GetModuleHandle(nullptr),       // Instance handle
        nullptr                         // Additional application data
    );

    if (hwnd == nullptr) {
        MessageBox(nullptr, "Window Creation Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
        return false;
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        if (g_guiSubmitted || g_guiCancelled) {
            break; // Exit message loop
        }
    }

    if (g_guiSubmitted) {
        src = g_srcPath;
        dest = g_destPath;
        samples = g_samplesDir;
        w = g_checkboardWidth;
        h = g_checkboardHeight;
    }

    return g_guiSubmitted;
}

// --- WINDOW PROCEDURE ---
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE: {
            int yPos = 20;
            int xPos = 20;
            int width = 600;
            int height = 24;
            int labelWidth = 150;
            int editWidth = 450;
            int gap = 50;

            // Source Image Path
            CreateWindow("STATIC", "Source Image Path:", WS_VISIBLE | WS_CHILD,
                         xPos, yPos, labelWidth, height, hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
            hSrcPathEdit = CreateWindow("EDIT", "example/samples/IMG-0.jpg", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
                                        xPos + labelWidth, yPos, editWidth, height, hwnd, (HMENU)IDC_SRC_PATH_EDIT, GetModuleHandle(nullptr), nullptr);
            yPos += gap;

            // Destination Image Path
            CreateWindow("STATIC", "Destination Image Path:", WS_VISIBLE | WS_CHILD,
                         xPos, yPos, labelWidth, height, hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
            hDestPathEdit = CreateWindow("EDIT", "undistorted.jpg", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
                                         xPos + labelWidth, yPos, editWidth, height, hwnd, (HMENU)IDC_DEST_PATH_EDIT, GetModuleHandle(nullptr), nullptr);
            yPos += gap;

            // Samples Directory
            CreateWindow("STATIC", "Samples Directory:", WS_VISIBLE | WS_CHILD,
                         xPos, yPos, labelWidth, height, hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
            hSamplesDirEdit = CreateWindow("EDIT", "example/samples", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
                                           xPos + labelWidth, yPos, editWidth, height, hwnd, (HMENU)IDC_SAMPLES_DIR_EDIT, GetModuleHandle(nullptr), nullptr);
            yPos += gap;

            // Checkboard Width
            CreateWindow("STATIC", "Checkboard Width (cols):", WS_VISIBLE | WS_CHILD,
                         xPos, yPos, labelWidth, height, hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
            hWidthEdit = CreateWindow("EDIT", "9", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL,
                                      xPos + labelWidth, yPos, editWidth / 2, height, hwnd, (HMENU)IDC_CHECKBOARD_WIDTH_EDIT, GetModuleHandle(nullptr), nullptr);
            yPos += gap;

            // Checkboard Height
            CreateWindow("STATIC", "Checkboard Height (rows):", WS_VISIBLE | WS_CHILD,
                         xPos, yPos, labelWidth, height, hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
            hHeightEdit = CreateWindow("EDIT", "6", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL,
                                       xPos + labelWidth, yPos, editWidth / 2, height, hwnd, (HMENU)IDC_CHECKBOARD_HEIGHT_EDIT, GetModuleHandle(nullptr), nullptr);
            yPos += gap + 20;

            // Start Button
            CreateWindow("BUTTON", "Start Calibration", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                         xPos, yPos, width, 40, hwnd, (HMENU)IDC_START_BUTTON, GetModuleHandle(nullptr), nullptr);

            break;
        }
        case WM_COMMAND: {
            if (LOWORD(wParam) == IDC_START_BUTTON) {
                char buffer[MAX_PATH]; // Use MAX_PATH for paths, smaller for numbers

                GetWindowText(hSrcPathEdit, buffer, MAX_PATH);
                g_srcPath = buffer;

                GetWindowText(hDestPathEdit, buffer, MAX_PATH);
                g_destPath = buffer;

                GetWindowText(hSamplesDirEdit, buffer, MAX_PATH);
                g_samplesDir = buffer;

                GetWindowText(hWidthEdit, buffer, 10); // Checkboard dimensions are small
                try {
                    g_checkboardWidth = std::stoi(buffer);
                } catch (const std::exception& e) {
                    MessageBox(hwnd, ("Invalid width input: " + std::string(e.what())).c_str(), "Error", MB_ICONERROR | MB_OK);
                    return 0;
                }

                GetWindowText(hHeightEdit, buffer, 10);
                try {
                    g_checkboardHeight = std::stoi(buffer);
                } catch (const std::exception& e) {
                    MessageBox(hwnd, ("Invalid height input: " + std::string(e.what())).c_str(), "Error", MB_ICONERROR | MB_OK);
                    return 0;
                }

                g_guiSubmitted = true;
                DestroyWindow(hwnd); // Close the GUI window
            }
            break;
        }
        case WM_CLOSE:
            g_guiCancelled = true;
            DestroyWindow(hwnd);
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}


// --- EXISTING CALIBRATION LOGIC ---

std::vector<cv::Point3f> calibratePattern(cv::Size checkboardSize, float squareSize) {
    std::vector<cv::Point3f> ret;
    for (int i = 0; i < checkboardSize.height; i++) {
        for (int j = 0; j < checkboardSize.width; j++) {
            ret.push_back(cv::Point3f(float(j * squareSize), float(i * squareSize), 0));
        }
    }
    return ret;
}

std::string promptForInput(const std::string& message) {
    std::cout << message;
    std::string input;
    std::getline(std::cin, input);
    return input;
}

int main(int argc, char** argv) {
    std::string srcPath, destPath, samplesDir;
    int checkboardWidth, checkboardHeight;
    bool useGui = false;

    // Check for GUI flag
    if (argc > 1 && std::string(argv[1]) == "-gui") {
        useGui = true;
    }

    if (useGui) {
        std::cout << "Launching GUI..." << std::endl;
        // The main function will not exit until the GUI window is closed.
        // It relies on g_guiSubmitted and g_guiCancelled globals set by WndProc.
        if (!runWinGuiMode(srcPath, destPath, samplesDir, checkboardWidth, checkboardHeight)) {
            std::cout << "GUI cancelled or exited." << std::endl;
            return 0; // Exit if GUI was cancelled or failed to initialize
        }
    }
    else if (argc > 1 && (std::string(argv[1]) == "--interactive" || std::string(argv[1]) == "-i")) {
        std::cout << "Entering interactive mode. Please provide the following inputs:" << std::endl;
        srcPath = promptForInput("Enter source image path: ");
        destPath = promptForInput("Enter destination image path: ");
        samplesDir = promptForInput("Enter samples directory path: ");
        checkboardWidth = std::stoi(promptForInput("Enter checkboard width: "));
        checkboardHeight = std::stoi(promptForInput("Enter checkboard height: "));
    } else {
        if (argc < 6 || argc > 7) { // 6 arguments for normal, 7 if first is -gui (which is handled above)
            std::cout << "Usage: ./fisheye <src_image> <dest_image> <samples_dir> <checkboard_width> <checkboard_height>" << std::endl;
            std::cout << "Or: ./fisheye -i (Interactive Mode)" << std::endl;
            std::cout << "Or: ./fisheye -gui (Window Mode)" << std::endl;
            return 1;
        }
        // Handle direct arguments if not GUI or interactive
        int argOffset = 0; // No offset if not -gui
        if (std::string(argv[1]) == "-gui") { // If -gui is present but not handled (e.g. if more args given later)
            std::cerr << "Error: -gui option should be used alone. Exiting." << std::endl;
            return 1;
        }

        srcPath = argv[1];
        destPath = argv[2];
        samplesDir = argv[3];
        checkboardWidth = std::stoi(argv[4]);
        checkboardHeight = std::stoi(argv[5]);
    }

    // 1. Load Calibration Images
    std::vector<cv::Mat> images;
    std::cout << "Loading samples from " << samplesDir << "..." << std::endl;
    
    if (!fs::exists(samplesDir)) {
        std::cerr << "Error: Samples directory '" << samplesDir << "' not found." << std::endl;
        return 1;
    }

    for (const auto& entry : fs::directory_iterator(samplesDir)) {
        std::string p = entry.path().string();
        std::string ext = entry.path().extension().string();
        // Convert ext to lowercase for comparison
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c){ return std::tolower(c); });

        // Simple check for jpg/png
        if (ext == ".jpg" || ext == ".png" || ext == ".jpeg" || ext == ".bmp") {
            cv::Mat img = cv::imread(p, cv::IMREAD_GRAYSCALE);
            if (!img.empty()) {
                images.push_back(img);
            }
        }
    }

    if (images.empty()) {
        std::cerr << "No images found in " << samplesDir << std::endl;
        if(useGui) std::system("pause"); // Keep window open to see error
        return 1;
    }
    std::cout << "Loaded " << images.size() << " images." << std::endl;

    // 2. Calibrate
    std::cout << "Calibrating..." << std::endl;
    cv::Size checkboardSize(checkboardWidth, checkboardHeight);
    std::vector<std::vector<cv::Point3f>> objPoints;
    std::vector<cv::Mat> imgPoints;
    std::vector<cv::Point3f> pattern = calibratePattern(checkboardSize, 1.0);
    cv::TermCriteria subpixCriteria(cv::TermCriteria::EPS | cv::TermCriteria::MAX_ITER, 30, 0.1);

    for (auto& img : images) {
        cv::Mat corners;
        // Use CALIB_CB_ADAPTIVE_THRESH for better robustness
        bool found = cv::findChessboardCorners(img, checkboardSize, corners,
            cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE);

        if (found) {
            objPoints.push_back(pattern);
            cv::cornerSubPix(img, corners, cv::Size(3, 3), cv::Size(-1, -1), subpixCriteria);
            imgPoints.push_back(corners);
        }
    }

    if (objPoints.empty()) {
        std::cerr << "Could not detect any checkboards with size " << checkboardWidth << "x" << checkboardHeight << std::endl;
        if(useGui) std::system("pause");
        return 1;
    }

    cv::Matx33d K;
    cv::Vec4d D;
    cv::Size size = images[0].size();
    int flag = cv::fisheye::CALIB_RECOMPUTE_EXTRINSIC | cv::fisheye::CALIB_CHECK_COND | cv::fisheye::CALIB_FIX_SKEW;
    cv::TermCriteria criteria(cv::TermCriteria::EPS | cv::TermCriteria::MAX_ITER, 30, 1e-6);
    
    double error = cv::fisheye::calibrate(objPoints, imgPoints, size, K, D, cv::noArray(), cv::noArray(), flag, criteria);
    std::cout << "Calibration done. Reprojection error: " << error << std::endl;

    // 3. Undistort
    std::cout << "Undistorting " << srcPath << "..." << std::endl;
    cv::Mat distorted = cv::imread(srcPath);
    if (distorted.empty()) {
        std::cerr << "Failed to read source image: " << srcPath << std::endl;
        if(useGui) std::system("pause");
        return 1;
    }

    cv::Mat undistorted;
    // K is used for both original and new camera matrix to keep the scale
    cv::fisheye::undistortImage(distorted, undistorted, K, D, K, distorted.size());

        // 4. Save

        if (cv::imwrite(destPath, undistorted)) {

            std::cout << "Saved to " << destPath << std::endl;

            // Show result if in GUI mode

            if (useGui) {

                // In WinAPI GUI mode, we should perhaps show the image in a new window

                // or inform the user it's saved. For simplicity, just confirmation.

                MessageBox(nullptr, ("Image saved to: " + destPath).c_str(), "Success", MB_OK | MB_ICONINFORMATION);

            }

        } else {

            std::cerr << "Failed to save image to " << destPath << std::endl;

            if(useGui) MessageBox(nullptr, ("Failed to save image to: " + destPath).c_str(), "Error", MB_OK | MB_ICONERROR);

            return 1;

        }



        return 0;

    }

