#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <stdio.h>
#pragma comment(lib,"user32.lib")
#pragma comment(lib,"d3d11.lib")
#pragma comment(lib,"dxgi.lib")
LRESULT CALLBACK Wnd(HWND h,UINT m,WPARAM w,LPARAM l){return DefWindowProcA(h,m,w,l);}
int main(){
  WNDCLASSA wc={}; wc.lpfnWndProc=Wnd; wc.hInstance=GetModuleHandle(0); wc.lpszClassName="T";
  RegisterClassA(&wc);
  HWND top=CreateWindowExA(0,"T","top",WS_OVERLAPPEDWINDOW|WS_VISIBLE,100,100,400,300,0,0,wc.hInstance,0);
  HWND child=CreateWindowExA(0,"STATIC","",WS_CHILD|WS_VISIBLE,10,10,200,120,top,0,wc.hInstance,0);
  ID3D11Device* dev=0; ID3D11DeviceContext* ctx=0;
  D3D11CreateDevice(0,D3D_DRIVER_TYPE_HARDWARE,0,D3D11_CREATE_DEVICE_BGRA_SUPPORT,0,0,D3D11_SDK_VERSION,&dev,0,&ctx);
  IDXGIDevice* dxgiDev=0; IDXGIAdapter* ad=0; IDXGIFactory2* fac=0;
  dev->QueryInterface(__uuidof(IDXGIDevice),(void**)&dxgiDev);
  dxgiDev->GetAdapter(&ad); ad->GetParent(__uuidof(IDXGIFactory2),(void**)&fac);
  DXGI_SWAP_CHAIN_DESC1 d={}; d.Width=200;d.Height=120;d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.SampleDesc.Count=1;d.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;d.BufferCount=2;d.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
  IDXGISwapChain1* swap=0;
  HRESULT hrChild=fac->CreateSwapChainForHwnd(dev,child,&d,0,0,&swap);
  printf("child HR=0x%08X swap=%p\n",(unsigned)hrChild,swap);
  if(swap){swap->Release();swap=0;}
  d.Width=380;d.Height=260;d.BufferCount=2;
  HRESULT hrTop=fac->CreateSwapChainForHwnd(dev,top,&d,0,0,&swap);
  printf("top HR=0x%08X swap=%p\n",(unsigned)hrTop,swap);
  if(swap)swap->Release();
  fac->Release();ad->Release();dxgiDev->Release();ctx->Release();dev->Release();
  DestroyWindow(top);
  return 0;
}
