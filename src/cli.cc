#include <opencv2/core.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp> // Added highgui header
#include <iostream>
#include <vector>
#include <filesystem>
#include <string>
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

// --- GUI HELPER STRUCTURES ---
struct InputField {
    std::string label;
    std::string value;
    cv::Rect rect;
    bool isNumeric;
};

struct GuiState {
    std::vector<InputField> fields;
    cv::Rect buttonRect;
    int activeFieldIndex = -1;
    bool submitted = false;
    bool shouldExit = false;
};

// Mouse callback to handle clicking on fields and button
void onMouse(int event, int x, int y, int flags, void* userdata) {
    GuiState* state = (GuiState*)userdata;
    if (event == cv::EVENT_LBUTTONDOWN) {
        state->activeFieldIndex = -1; // Deselect

        // Check Fields
        for (size_t i = 0; i < state->fields.size(); ++i) {
            if (state->fields[i].rect.contains(cv::Point(x, y))) {
                state->activeFieldIndex = (int)i;
                return;
            }
        }

        // Check Submit Button
        if (state->buttonRect.contains(cv::Point(x, y))) {
            state->submitted = true;
        }
    }
}

// Function to draw the form
void drawForm(cv::Mat& canvas, const GuiState& state) {
    canvas = cv::Scalar(50, 50, 50); // Dark grey background

    int fontFace = cv::FONT_HERSHEY_SIMPLEX;
    double fontScale = 0.5;
    int thickness = 1;
    int padding = 10;

    for (size_t i = 0; i < state.fields.size(); ++i) {
        const auto& f = state.fields[i];

        // Draw Label
        cv::putText(canvas, f.label, cv::Point(f.rect.x, f.rect.y - 8), fontFace, fontScale, cv::Scalar(200, 200, 200), thickness);

        // Draw Input Box Background
        cv::Scalar boxColor = (state.activeFieldIndex == (int)i) ? cv::Scalar(100, 100, 100) : cv::Scalar(70, 70, 70);
        cv::Scalar borderColor = (state.activeFieldIndex == (int)i) ? cv::Scalar(0, 255, 0) : cv::Scalar(150, 150, 150);

        cv::rectangle(canvas, f.rect, boxColor, cv::FILLED);
        cv::rectangle(canvas, f.rect, borderColor, 1);

        // Draw Value text
        std::string displayParams = f.value;
        // Simple cursor simulation
        if (state.activeFieldIndex == (int)i) displayParams += "|";

        // Clip text if too long (very basic clipping)
        if (displayParams.length() > 55) displayParams = "..." + displayParams.substr(displayParams.length() - 52);

        cv::putText(canvas, displayParams, cv::Point(f.rect.x + 5, f.rect.y + 20), fontFace, fontScale, cv::Scalar(255, 255, 255), thickness);
    }

    // Draw Button
    cv::rectangle(canvas, state.buttonRect, cv::Scalar(200, 100, 0), cv::FILLED); // Blueish
    cv::rectangle(canvas, state.buttonRect, cv::Scalar(255, 255, 255), 1);

    std::string btnText = "START CALIBRATION";
    int baseline = 0;
    cv::Size txtSize = cv::getTextSize(btnText, fontFace, 0.7, 2, &baseline);
    cv::Point textOrg((state.buttonRect.width - txtSize.width) / 2 + state.buttonRect.x,
                      (state.buttonRect.height + txtSize.height) / 2 + state.buttonRect.y);
    cv::putText(canvas, btnText, textOrg, fontFace, 0.7, cv::Scalar(255, 255, 255), 2);
}

bool runGuiMode(std::string& src, std::string& dest, std::string& samples, int& w, int& h) {
    GuiState state;
    int startY = 50;
    int gapY = 60;
    int width = 580;
    int height = 30;
    int startX = 30;

    // Define Layout
    state.fields.push_back({"Source Image Path", "example/samples/IMG-0.jpg", cv::Rect(startX, startY, width, height), false});
    state.fields.push_back({"Destination Image Path", "undistorted.jpg", cv::Rect(startX, startY + gapY, width, height), false});
    state.fields.push_back({"Samples Directory", "example/samples", cv::Rect(startX, startY + gapY*2, width, height), false});
    state.fields.push_back({"Checkboard Width (cols)", "9", cv::Rect(startX, startY + gapY*3, width/2 - 10, height), true});
    state.fields.push_back({"Checkboard Height (rows)", "6", cv::Rect(startX + width/2 + 10, startY + gapY*3, width/2 - 10, height), true});

    state.buttonRect = cv::Rect(startX, startY + gapY * 4 + 20, width, 50);

    cv::Mat canvas(400, 640, CV_8UC3);
    cv::namedWindow("Fisheye Calibrator Input", cv::WINDOW_AUTOSIZE);
    cv::setMouseCallback("Fisheye Calibrator Input", onMouse, &state);

    while (!state.submitted) {
        drawForm(canvas, state);
        cv::imshow("Fisheye Calibrator Input", canvas);

        int key = cv::waitKey(20);

        // Check if window closed
        if (cv::getWindowProperty("Fisheye Calibrator Input", cv::WND_PROP_AUTOSIZE) == -1) {
            return false;
        }

        if (key == 27) return false; // ESC to exit

        // Handle Text Input
        if (state.activeFieldIndex >= 0 && key != -1) {
            std::string& val = state.fields[state.activeFieldIndex].value;

            if (key == 8) { // Backspace
                if (!val.empty()) val.pop_back();
            }
            else if (key == 13) { // Enter
                state.activeFieldIndex = -1; // confirm
            }
            else if (key >= 32 && key <= 126) { // Printable chars
                // If numeric field, restrict input
                if (state.fields[state.activeFieldIndex].isNumeric) {
                    if (isdigit(key)) val += (char)key;
                } else {
                    val += (char)key;
                }
            }
        }
    }

    cv::destroyWindow("Fisheye Calibrator Input");

    // Assign back to variables
    src = state.fields[0].value;
    dest = state.fields[1].value;
    samples = state.fields[2].value;
    try {
        w = std::stoi(state.fields[3].value);
        h = std::stoi(state.fields[4].value);
    } catch (...) {
        std::cerr << "Invalid width/height provided via GUI." << std::endl;
        return false;
    }

    return true;
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
        if (!runGuiMode(srcPath, destPath, samplesDir, checkboardWidth, checkboardHeight)) {
            std::cout << "GUI cancelled or exited." << std::endl;
            return 0;
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
        if (argc != 6) {
            std::cout << "Usage: ./fisheye <src_image> <dest_image> <samples_dir> <checkboard_width> <checkboard_height>" << std::endl;
            std::cout << "Or: ./fisheye -i (Interactive Mode)" << std::endl;
            std::cout << "Or: ./fisheye -gui (Window Mode)" << std::endl;
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
            cv::imshow("Result", undistorted);
            std::cout << "Press any key to exit..." << std::endl;
            cv::waitKey(0);
        }
    } else {
        std::cerr << "Failed to save image to " << destPath << std::endl;
        if(useGui) std::system("pause");
        return 1;
    }

    return 0;
}