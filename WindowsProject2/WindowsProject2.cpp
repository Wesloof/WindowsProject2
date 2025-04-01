#include "framework.h"
#include "WindowsProject2.h"
#include <vector>
#include <algorithm>
#include <string>

#define MAX_LOADSTRING 100
#define GRAPH_HEIGHT 80

// Global Variables
HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];

// Image variables
HBITMAP hOriginalBitmap = NULL;
std::vector<HBITMAP> processedBitmaps;
int imageWidth = 0;
int imageHeight = 0;

// Function declarations
ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);
void LoadImage(HWND hWnd);
void ProcessImages();
void DrawImages(HDC hdc);
void CleanUp();
HBITMAP SmoothImage(HBITMAP source, int kernelSize, int passes);
HBITMAP CreateDitheredImage(HBITMAP source, bool chessPattern);
void DrawProfile(HDC hdc, int x, int y, HBITMAP bitmap, int width, int height);
COLORREF RGBToGray(COLORREF color);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_WINDOWSPROJECT2, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_WINDOWSPROJECT2));
    MSG msg;

    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    CleanUp();
    return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WINDOWSPROJECT2));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_WINDOWSPROJECT2);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance;
    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, 1000, 800, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    LoadImage(hWnd);
    ProcessImages();

    return TRUE;
}

void LoadImage(HWND hWnd)
{
    if (hOriginalBitmap)
    {
        DeleteObject(hOriginalBitmap);
        hOriginalBitmap = NULL;
    }

    hOriginalBitmap = (HBITMAP)LoadImage(NULL, L"image.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
    if (!hOriginalBitmap)
    {
        MessageBox(hWnd, L"Failed to load image.bmp", L"Error", MB_ICONERROR);
        return;
    }

    BITMAP bm;
    GetObject(hOriginalBitmap, sizeof(BITMAP), &bm);
    imageWidth = bm.bmWidth;
    imageHeight = bm.bmHeight;
}

void ProcessImages()
{
    // Clear previous processed images
    for (HBITMAP hbm : processedBitmaps)
    {
        DeleteObject(hbm);
    }
    processedBitmaps.clear();

    if (!hOriginalBitmap) return;

    // Create processed versions
    processedBitmaps.push_back(SmoothImage(hOriginalBitmap, 3, 1));  // 3x3 1 pass
    processedBitmaps.push_back(SmoothImage(hOriginalBitmap, 3, 2));  // 3x3 2 passes
    processedBitmaps.push_back(SmoothImage(hOriginalBitmap, 3, 3));  // 3x3 3 passes
    processedBitmaps.push_back(SmoothImage(hOriginalBitmap, 5, 1));  // 5x5 1 pass
    processedBitmaps.push_back(SmoothImage(hOriginalBitmap, 5, 2));  // 5x5 2 passes
    processedBitmaps.push_back(SmoothImage(hOriginalBitmap, 5, 3));  // 5x5 3 passes
    processedBitmaps.push_back(CreateDitheredImage(hOriginalBitmap, false)); // Linear
    processedBitmaps.push_back(CreateDitheredImage(hOriginalBitmap, true));  // Chess
}

HBITMAP SmoothImage(HBITMAP source, int kernelSize, int passes)
{
    HDC hdcScreen = GetDC(NULL);
    HDC hdcSrc = CreateCompatibleDC(hdcScreen);
    HDC hdcDst = CreateCompatibleDC(hdcScreen);

    HBITMAP result = CreateCompatibleBitmap(hdcScreen, imageWidth, imageHeight);
    HBITMAP hOldSrc = (HBITMAP)SelectObject(hdcSrc, source);
    HBITMAP hOldDst = (HBITMAP)SelectObject(hdcDst, result);

    // Copy original first
    BitBlt(hdcDst, 0, 0, imageWidth, imageHeight, hdcSrc, 0, 0, SRCCOPY);

    int radius = kernelSize / 2;
    std::vector<COLORREF> pixels(imageWidth * imageHeight);

    for (int p = 0; p < passes; p++)
    {
        // Read current state
        for (int y = 0; y < imageHeight; y++)
            for (int x = 0; x < imageWidth; x++)
                pixels[y * imageWidth + x] = GetPixel(hdcDst, x, y);

        // Apply smoothing
        for (int y = radius; y < imageHeight - radius; y++)
        {
            for (int x = radius; x < imageWidth - radius; x++)
            {
                int r = 0, g = 0, b = 0, count = 0;

                for (int dy = -radius; dy <= radius; dy++)
                {
                    for (int dx = -radius; dx <= radius; dx++)
                    {
                        int nx = x + dx;
                        int ny = y + dy;
                        if (nx >= 0 && nx < imageWidth && ny >= 0 && ny < imageHeight)
                        {
                            COLORREF color = pixels[ny * imageWidth + nx];
                            r += GetRValue(color);
                            g += GetGValue(color);
                            b += GetBValue(color);
                            count++;
                        }
                    }
                }

                if (count > 0)
                {
                    SetPixel(hdcDst, x, y, RGB(r / count, g / count, b / count));
                }
            }
        }
    }

    SelectObject(hdcSrc, hOldSrc);
    SelectObject(hdcDst, hOldDst);
    DeleteDC(hdcSrc);
    DeleteDC(hdcDst);
    ReleaseDC(NULL, hdcScreen);

    return result;
}

HBITMAP CreateDitheredImage(HBITMAP source, bool chessPattern)
{
    HDC hdcScreen = GetDC(NULL);
    HDC hdcSrc = CreateCompatibleDC(hdcScreen);
    HDC hdcDst = CreateCompatibleDC(hdcScreen);

    HBITMAP result = CreateCompatibleBitmap(hdcScreen, imageWidth, imageHeight);
    HBITMAP hOldSrc = (HBITMAP)SelectObject(hdcSrc, source);
    HBITMAP hOldDst = (HBITMAP)SelectObject(hdcDst, result);

    // Patterns
    const int linearPattern[3][3] = { {0,3,1},{4,2,5},{7,6,8} };
    const int chessPatternMatrix[3][3] = { {0,5,2},{7,4,3},{6,1,8} };

    for (int y = 0; y < imageHeight; y++)
    {
        for (int x = 0; x < imageWidth; x++)
        {
            COLORREF color = GetPixel(hdcSrc, x, y);
            int gray = GetRValue(RGBToGray(color));
            int level = gray * 9 / 256;

            // Apply dithering pattern
            int threshold = chessPattern ? chessPatternMatrix[y % 3][x % 3] : linearPattern[y % 3][x % 3];
            COLORREF newColor = threshold < level ? RGB(255, 255, 255) : RGB(0, 0, 0);
            SetPixel(hdcDst, x, y, newColor);
        }
    }

    SelectObject(hdcSrc, hOldSrc);
    SelectObject(hdcDst, hOldDst);
    DeleteDC(hdcSrc);
    DeleteDC(hdcDst);
    ReleaseDC(NULL, hdcScreen);

    return result;
}

COLORREF RGBToGray(COLORREF color)
{
    int r = GetRValue(color);
    int g = GetGValue(color);
    int b = GetBValue(color);
    return RGB((r * 30 + g * 59 + b * 11) / 100, (r * 30 + g * 59 + b * 11) / 100, (r * 30 + g * 59 + b * 11) / 100);
}

void DrawProfile(HDC hdc, int x, int y, HBITMAP bitmap, int width, int height)
{
    if (!bitmap) return;

    // Create compatible DC and select our bitmap
    HDC hdcMem = CreateCompatibleDC(hdc);
    HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcMem, bitmap);

    // Get image bits directly for reliable access
    BITMAP bm;
    GetObject(bitmap, sizeof(BITMAP), &bm);

    // Create buffer to hold pixel data
    std::vector<BYTE> pixels(bm.bmWidthBytes * bm.bmHeight);
    GetBitmapBits(bitmap, bm.bmWidthBytes * bm.bmHeight, pixels.data());

    // Prepare intensities array
    std::vector<int> intensities(width);
    int midY = height / 2;

    // Calculate intensities for middle row
    for (int i = 0; i < width; i++)
    {
        int offset = midY * bm.bmWidthBytes + i * (bm.bmBitsPixel / 8);
        if (offset + 2 < (int)pixels.size()) // Ensure we don't go out of bounds
        {
            BYTE blue = pixels[offset];
            BYTE green = pixels[offset + 1];
            BYTE red = pixels[offset + 2];
            intensities[i] = (red * 30 + green * 59 + blue * 11) / 100; // Grayscale value
        }
    }

    // Draw graph background
    RECT graphRect = { x, y, x + width, y + GRAPH_HEIGHT };
    FillRect(hdc, &graphRect, (HBRUSH)GetStockObject(WHITE_BRUSH));
    FrameRect(hdc, &graphRect, (HBRUSH)GetStockObject(BLACK_BRUSH));

    // Draw graph line
    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(255, 0, 0));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

    for (int i = 0; i < width - 1; i++)
    {
        int y1 = y + GRAPH_HEIGHT - (intensities[i] * GRAPH_HEIGHT / 255);
        int y2 = y + GRAPH_HEIGHT - (intensities[i + 1] * GRAPH_HEIGHT / 255);
        MoveToEx(hdc, x + i, y1, NULL);
        LineTo(hdc, x + i + 1, y2);
    }

    // Cleanup
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);
    SelectObject(hdcMem, hOldBmp);
    DeleteDC(hdcMem);
}

void DrawImages(HDC hdc)
{
    if (!hOriginalBitmap) return;

    int spacing = 20;
    int startX = 20;
    int startY = 20;
    int cols = 3;

    // Draw original
    HDC hdcMem = CreateCompatibleDC(hdc);
    HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcMem, hOriginalBitmap);
    BitBlt(hdc, startX, startY, imageWidth, imageHeight, hdcMem, 0, 0, SRCCOPY);
    DrawProfile(hdc, startX, startY + imageHeight + 5, hOriginalBitmap, imageWidth, imageHeight);
    SelectObject(hdcMem, hOldBmp);
    DeleteDC(hdcMem);

    // Draw processed images
    for (size_t i = 0; i < processedBitmaps.size(); i++)
    {
        int row = (i + 1) / cols;
        int col = (i + 1) % cols;
        int x = startX + (imageWidth + spacing) * col;
        int y = startY + (imageHeight + spacing + GRAPH_HEIGHT + 10) * row;

        hdcMem = CreateCompatibleDC(hdc);
        hOldBmp = (HBITMAP)SelectObject(hdcMem, processedBitmaps[i]);
        BitBlt(hdc, x, y, imageWidth, imageHeight, hdcMem, 0, 0, SRCCOPY);
        DrawProfile(hdc, x, y + imageHeight + 5, processedBitmaps[i], imageWidth, imageHeight);
        SelectObject(hdcMem, hOldBmp);
        DeleteDC(hdcMem);
    }
}

void CleanUp()
{
    if (hOriginalBitmap) DeleteObject(hOriginalBitmap);
    for (HBITMAP hbm : processedBitmaps)
    {
        if (hbm) DeleteObject(hbm);
    }
    processedBitmaps.clear();
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        switch (wmId)
        {
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;
    case WM_KEYDOWN:
        if (wParam == 'R' && GetKeyState(VK_CONTROL) < 0)
        {
            LoadImage(hWnd);
            ProcessImages();
            InvalidateRect(hWnd, NULL, TRUE);
        }
        break;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        DrawImages(hdc);
        EndPaint(hWnd, &ps);
    }
    break;
    case WM_DESTROY:

        CleanUp();
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}