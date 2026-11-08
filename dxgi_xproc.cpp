#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <stdio.h>
#pragma comment(lib,"user32.lib")
#pragma comment(lib,"d3d11.lib")
#pragma comment(lib,"dxgi.lib")
static HWND g_child=0;
LRESULT CALLBACK Wnd(HWND h,UINT m,WPARAM w,LPARAM l){
  if(m==WM_CREATE){
    g_child=CreateWindowExA(0,"STATIC","",WS_CHILD|WS_VISIBLE,10,10,320,180,h,0,GetModuleHandle(0),0);
    printf("HOST child=%p pid=%lu\n",g_child,GetCurrentProcessId());
    fflush(stdout);
  }
  if(m==WM_DESTROY){PostQuitMessage(0);}
  return DefWindowProcA(h,m,w,l);
}
int main(int argc,char**argv){
  if(argc>1){
    HWND hwnd=(HWND)(UINT_PTR)_strtoui64(argv[1],0,16);
    printf("GPU trying hwnd=%p from pid=%lu\n",hwnd,GetCurrentProcessId());
    ID3D11Device*dev=0;ID3D11DeviceContext*ctx=0;
    D3D11CreateDevice(0,D3D_DRIVER_TYPE_HARDWARE,0,D3D11_CREATE_DEVICE_BGRA_SUPPORT,0,0,D3D11_SDK_VERSION,&dev,0,&ctx);
    IDXGIDevice*dxgiDev=0;IDXGIAdapter*ad=0;IDXGIFactory2*fac=0;
    dev->QueryInterface(__uuidof(IDXGIDevice),(void**)&dxgiDev);
    dxgiDev->GetAdapter(&ad);ad->GetParent(__uuidof(IDXGIFactory2),(void**)&fac);
    RECT rc={};GetClientRect(hwnd,&rc);
    DXGI_SWAP_CHAIN_DESC1 d={};d.Width=rc.right;d.Height=rc.bottom;d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.SampleDesc.Count=1;d.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;d.BufferCount=2;d.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
    IDXGISwapChain1*swap=0;HRESULT hr=fac->CreateSwapChainForHwnd(dev,hwnd,&d,0,0,&swap);
    printf("cross HR=0x%08X swap=%p size=%dx%d IsWindow=%d\n",(unsigned)hr,swap,rc.right,rc.bottom,(int)IsWindow(hwnd));
    fflush(stdout);
    if(swap)swap->Release();fac->Release();ad->Release();dxgiDev->Release();ctx->Release();dev->Release();
    return FAILED(hr)?1:0;
  }
  WNDCLASSA wc={};wc.lpfnWndProc=Wnd;wc.hInstance=GetModuleHandle(0);wc.lpszClassName="HostT";
  RegisterClassA(&wc);
  HWND top=CreateWindowExA(0,"HostT","host",WS_OVERLAPPEDWINDOW|WS_VISIBLE,80,80,500,400,0,0,wc.hInstance,0);
  MSG msg; while(GetMessage(&msg,0,0,0)){TranslateMessage(&msg);DispatchMessage(&msg);}
  return 0;
}
