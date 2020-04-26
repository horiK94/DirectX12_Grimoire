#include <Windows.h>
#include <tchar.h>
#include <d3d12.h>		//DX12の3D用ライブラリ
#include <dxgi1_6.h>		//DXGI(Direct3D APIよりもドライバーに近いディスプレイ出力に直接関係する機能を制御するためのAPIセット)
#include <vector>
#include <DirectXMath.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

#ifdef  _DEBUG
#include <iostream>
#endif //  _DEBUG

using namespace std;
using namespace DirectX;

// @brief: コンソール画面にフォーマット付き文字列を表示
// @param format フォーマット (%f, %dなど)
// @param 可変長引数
// @remaks この関数はデバッグ用.デバッグ時のみ動作
void DebugOutputFormatString(const char* format, ...)
{
	// DebugOutputFormatString("%d %f %s\n", 100, 3.14, "piyo");のように使えるようにする
#ifdef _DEBUG
	va_list valist;
	va_start(valist, format);		//可変長引数(上で言う 100, 3.14, "piyo")を1つの変数(valist)にまとめる
	printf(format, valist);
	va_end(valist);
#endif
}

const unsigned int window_width = 1280;
const unsigned int window_height = 720;

//コマンドリストの参照(GPUに対する命令をまとめるためのオブジェクト)
ID3D12GraphicsCommandList* _commandList = nullptr;
//コマンドアロケーターの参照(コマンドリストを使用するのに必要)
//コマンドリストに格納する命令の為のメモリを管理するオブジェクト
ID3D12CommandAllocator* _commandAllocator = nullptr;
//コマンドキューの参照
ID3D12CommandQueue* _commandQueue = nullptr;

//基本オブジェクトの変数
ID3D12Device* _dev = nullptr;		//デバイスオブジェクト(デバイスに関する参照)
//IDXGIFactory6: 特定のGPU設定に基づいてグラフィックアダプターを列挙する単一のメソッドが有効になるインターフェース
//このインターフェースは 
// IDXGIFactory6 :: EnumAdapterByGpuPreference(): 指定されたGPU設定に基づいてグラフィックアダプターを列挙します。
// という関数を実装する
IDXGIFactory6* _dxgiFactory = nullptr;
IDXGISwapChain4* _swapchain = nullptr;

//ウィンドウアプリケーションはマウス操作やキーボードなどプレイヤーの発生させたイベントを契機にして、次の処理を行う(イベント駆動型プログラミング)
LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	//ウィンドウが破棄される時に発生
	if (msg == WM_DESTROY)
	{
		//OSに対して、アプリが終了することを伝える
		PostQuitMessage(0);
		return 0;
	}
	//規定の処理を行う(興味のないメッセージの処理は，既定のウィンドウプロシージャである DefWindowProc に丸投げ)
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

///デバッグレイヤーを有効化
void EnableDebugLayer()
{
	//ID3D12Debug: デバッグ設定をしたり、パイプラインステートの検証を行うためのインタフェース
	ID3D12Debug* debugLayer = nullptr;
	//D3D12GetDebugInterface: デバックインターフェースを取得
	auto result = D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer));
	//EnableDebugLayer(): デバッグレイヤーの有効
	debugLayer->EnableDebugLayer();		//デバッグレイヤーの有効化
	debugLayer->Release();		//有効化したので、インターフェースを破棄
}

#if _DEBUG		//デバッグモード(最初の方はコマンドライン出力で情報表示するため)
int main()
{
#else // _DEBUG
#include<Windows.h>		//これがないと何故か _T("DX12サンプル"), で日本語を使うとエラーになる(上でも定義しているんだけどな....)
// Windowsプログラムの主要なインクルードファイルはWINDOWS.H
// Windowsプログラムにmain()関数は無い. エントリーポイントはWinMain()
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)		//非デバッグモード
{
#endif
	DebugOutputFormatString("Show windows test");

	//WNDCLASSEX 構造体でウインドウクラスのパラメタを指定
	//RegisterClassEx()で そのウインドウクラスを登録
	//CreateWindow()で登録したクラスを元にウインドウを作る
	//ShowWindow()で表示する．というのが一連の流れ
	WNDCLASSEX w = {};

	//WNDCLASSEXの構造体のサイズ(UINT)
	w.cbSize = sizeof(WNDCLASSEX);
	//このウインドウのメッセージを処理するコールバック関数へのポインタ(WNDPROC)
	w.lpfnWndProc = (WNDPROC)WindowProcedure;
	//このウインドウクラスにつける名前(適当で良い)(LPCTSTR)
	w.lpszClassName = _T("DX12Sample");
	//ハンドルの取得[このクラスのためのウインドウプロシージャがあるインスタンスハンドル](引数のhinstanceをそのまま代入しているサイトも有る)
	w.hInstance = GetModuleHandle(nullptr);

	//ウインドウクラスの登録(こういうの作るからよろしくってOSに予告する)
	RegisterClassEx(&w);

	//ウィンドウサイズの決定
	RECT wrc = {
		0,		//四角形の左上のx座標位置
		0,		//四角形の左上のy座標位置
		window_width,		//横幅
		window_height		//縦幅
	};

	//ウィンドウのサイズ補正
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	//ウィンドウの生成
	//返り値: 作成されたウィンドウのハンドル. 失敗したらNULL
	HWND hwnd = CreateWindow(
		w.lpszClassName,		//RegisterClassEx()で登録したクラスの名前(WNDCLASS.lpszClassName)か，定義済みのクラス
		_T("DX12サンプル"),		//タイトルバーの文字
		WS_OVERLAPPEDWINDOW,		//ウインドウスタイル. WS_OVERLAPPEDWINDOWに関しては上参考
		CW_USEDEFAULT,			//ウインドウ左上x座標(画面左上が0)．適当でいい時はCW_USEDEFAULT
		CW_USEDEFAULT,			//ウインドウ左上y座標(画面左上が0)．適当でいい時はCW_USEDEFAULT
		wrc.right - wrc.left,		//ウインドウ幅．適当でいい時はCW_USEDEFAULT
		wrc.bottom - wrc.top,		//ウインドウ高さ．適当でいい時はCW_USEDEFAULT
		nullptr,		//親ウインドウのハンドル．なければNULL．3つめの引数でDW_CHILDを指定したときは必須．
		nullptr,		//メニューのハンドル．デフォルト(WNDCLASS.lpszMenuName)を使う場合はNULL．
		w.hInstance, 	//ウインドウとかを作成するモジュール(呼び出しアプリケーションモジュール)のインスタンスのハンドル
		nullptr			//追加パラメータ
	);

#ifdef _DEBUG
	//デバッグレイヤー有効化
	EnableDebugLayer();
#endif // DEBUG


	/////////////////////////////
	// DX12周りの初期化
	/////////////////////////////

	//フィーチャーレベルの列挙(選択されたグラフィックドライバーが対応していないフィーチャーレベルだったら下げていくために)
	D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};

	//グラフィックボードが複数刺さっていることを考慮して、アダプターを列挙していく
	if (FAILED(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&_dxgiFactory))))
	{
		//だめだったので、通常のモードででDXGIオブジェクトを取得しに行く
		//DXGI_CREATE_FACTORY_DEBUGフラグでだめだったときよう.ただ、DirectX12でフラグを0にするのは推奨されていない気がする
		if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&_dxgiFactory))))
		{
			return -1;
		}
	}

	//グラフィックボードの使用できるアダプターを列挙していく
	vector<IDXGIAdapter*> adapters;

	//検索できたアダプターが入ってくる
	IDXGIAdapter* tmpAdapter = nullptr;
	//EnumAdapters(): Enumerates the adapters (video cards).ビデオカードアダプタを列挙したものを返す関数
	//成功時はS_OK. ローカルシステムに刺さっているアダプターより多い数のindexが来たらDXGI_ERROR_NOT_FOUND, 第2引数がnullptrならDXGI_ERROR_INVALID_CALL を返す(いずれもHRESULT型)
	for (int i = 0; _dxgiFactory->EnumAdapters(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; ++i)
	{
		adapters.push_back(tmpAdapter);
	}

	for (auto adapter : adapters)
	{
		//アダプターの名前を元に使用するアダプターを決めていく

		//DXGI1.0までのアダプターかビデオカードの説明用構造体
		//Describes an adapter (or video card) by using DXGI 1.0.
		DXGI_ADAPTER_DESC  adesc = {};		//初期化
		adapter->GetDesc(&adesc);		//アダプターから説明情報を取得

		std::wstring strDesc = adesc.Description;

		//探したいアダプターの名前で確認
		/*
		ワイド文字列リテラル(1文字あたりのバイト数を通常より多くしたデータ型)の時は、
		文字列リテラルに対して L をつける必要がある
		*/
		//std::string::npos: string::findで値が見つからなかった場合に返す値として定義されている
		if (strDesc.find(L"NVIDIA") != std::string::npos)		//ソフトウェアアダプター名に指定した文字列があるなら
		{
			//見つけられたので代入
			tmpAdapter = adapter;
			break;
		}
	}

	/////////////////////////////
	// DX3Dのデバイス初期化
	/////////////////////////////
	D3D_FEATURE_LEVEL featureLevel;		//対応しているフィーチャーレベルのうち、最も良かったものが代入される
	for (auto level : levels)
	{
		//デバイスオブジェクトの作成関数(成功時はS_OKを返す)
		if (D3D12CreateDevice(tmpAdapter,		//グラフィックドライバーのアダプターのポインタ(nullptrにすると自動で選択される)
			//複数のグラフィックドライバーが刺さっている場合は自動で選ばれたアダプターが最適なものとは限らないので注意

			level,			//最低限必要なフィーチャーレベル
			IID_PPV_ARGS(&_dev))
			== S_OK)
		{
			featureLevel = level;
			break;
		}
	}

	//作成結果とかを代入する変数
	HRESULT result;

	/////////////////////////////
	// コマンドアロケーターの作成
	/////////////////////////////
	result = _dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_commandAllocator));

	//コマンドリストの作成
	result = _dev->CreateCommandList(0, //	シングルGPUオペレーションの場合、これをゼロに設定
		D3D12_COMMAND_LIST_TYPE_DIRECT, //作成するコマンドリストのタイプ
		_commandAllocator,		//デバイスがコマンドリストを作成するコマンドアロケーターオブジェクトへのポインター
		nullptr,	// パイプライン状態オブジェクトへのオプションポインター
		IID_PPV_ARGS(&_commandList)		//作成されたコマンドリストが返される変数
	);

	/////////////////////////////
	// コマンドキューの実体の作成
	/////////////////////////////

	//コマンドキューの設定
	//D3D12_COMMAND_QUEUE_DESC構造
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {};

	//タイムアウトなし
	//(デフォルトのコマンドキュー)
	commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

	//アダプターを1つしか使わないので0
	/*
	シングルGPU操作の場合、これをゼロに設定.
	複数のGPUノードがある場合は、ビットを設定して、コマンドキューが適用されるノード（デバイスの物理アダプター）を識別
	*/
	commandQueueDesc.NodeMask = 0;

	//プライオリティは指定なし
	//コマンドキューの 優先度
	commandQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;

	//種類はコマンドリストと同様にする
	commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	//キュー作成
	result = _dev->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&_commandQueue));

	/////////////////////////////
	// スワップチェーンの作成
	/////////////////////////////

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width = window_width;
	swapChainDesc.Height = window_height;
	swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;		//RGBAに8byteずつ使用する(256bit)
	swapChainDesc.Stereo = false;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
	swapChainDesc.BufferCount = 2;

	//フレームバッファはウィンドウのサイズに引き伸ばす
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;

	//フレーム表示後はバッファーの中身を破棄
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	//アルファ値はGPUにおまかせ
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

	//ウィンドウ <-> フルスクリーンモード で切替可能に設定
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	//スワップチェーンの生成
	result = _dxgiFactory->CreateSwapChainForHwnd(
		_commandQueue,		//コマンドキューオブジェクト
		hwnd,				//ウィンドウハンドル
		&swapChainDesc,		//スワップチェーン設定
		nullptr,			//DXGI_SWAP_CHAIN_FULLSCREEN_DESC(スワップチェーンのフルスクリーン)構造体のポインタ(指定しない場合はnullptr)
		nullptr,			//IDXGIOutputインタフェースのポインタ(マルチモニターを使用する場合は、出力先のモニター指定のために使用する. 使用しないならnullptr)
		(IDXGISwapChain1**)&_swapchain		//スワップチェーン作成成功時にこの変数にインターフェースのポインタが格納される
	);

	//スワップチェーンを作成したが、スワップチェーンのバックバッファーをクリアしたり、書き込んだりはこのままではできない
	/*レンダータッゲットビューというビューが必要
		↓
	  レンダーターゲットビュー(ビューつまりディスクリプタのこと)を入れるディスクリプタヒープが必要
	*/

	/////////////////////////////
	// ディスクリプタヒープの作成(ビューはディスクリプタヒープがないと作れないため)
	/////////////////////////////

	//ディスクリプタヒープはディスクリプタを複数入れるためのヒープ領域

	//ヒープを作成するための設定
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	//今回のディスクリプタはレンダーターゲットビューとして使用
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	//GPUは1つしか使用しないので0
	heapDesc.NodeMask = 0;
	//表画面と裏画面の2つのバッファーに対するビューなので2を指定
	heapDesc.NumDescriptors = 2;
	//シェーダー側からディスクリプタのビューの情報を参照しないのでD3D12_DESCRIPTOR_HEAP_FLAG_NONEを指定
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	ID3D12DescriptorHeap* rtvHeap = nullptr;

	result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeap));

	/////////////////////////////
	// ディスクリプタヒープ内のディスクリプタとバックバッファーの関連付け
	/////////////////////////////

	//スワップチェーンのバックバッファーの数を取得したいために、スワップチェーンの説明を取得する
	DXGI_SWAP_CHAIN_DESC swapChaDesc = {};
	result = _swapchain->GetDesc(&swapChaDesc);			//この関数使うよりはIDXGISwapChain1::GetSwap1()を使ってねとのこと

	vector<ID3D12Resource*> _backBuffers(swapChaDesc.BufferCount);		//vector<T> t(n);		でT型の変数tを配列要素nで確保ということになる

	//ディスクリプタヒープの先頭のアドレスを受け取る
	D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeap->GetCPUDescriptorHandleForHeapStart();

	for (int idx = 0; idx < swapChaDesc.BufferCount; idx++)
	{
		//スワップチェーン内のバッファーとビューの関連付け

		//スワップチェーン内のバックバッファーのメモリの取得
		result = _swapchain->GetBuffer(idx, IID_PPV_ARGS(&_backBuffers[idx]));

		//(スワップチェーン内の)バックバッファーとビュー(ディスクリプタ)の関連付け
		//デスクリプタヒープ上にRTV用のデスクリプタを作成する関数
		_dev->CreateRenderTargetView(
			_backBuffers[idx],
			nullptr,
			handle
		);

		//ID3D12Device::GetDescriptorHandleIncrementSize(): ディスクリプタ1つ1つのサイズを返す
		//引数にはディスクリプタの種類(D3D12_DESCRIPTOR_HEAP_TYPE)を指定(レンダーターゲットビューはD3D12_DESCRIPTOR_HEAP_TYPE_RTV)
		handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	/////////////////////////////
	// フェンスの作成(GPUの処理が終わったかを監視するもの)
	/////////////////////////////
	//フェンスのインスタンス
	ID3D12Fence* _fence = nullptr;
	//フェンス初期化値
	UINT64 _fenceVal = 0;
	//フェンスの作成(これで、GPUの命令がすべて完了したかが知れる)
	result = _dev->CreateFence(_fenceVal,
		D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&_fence));

	//ウィンドウの表示
	ShowWindow(hwnd, SW_SHOW);

	// 頂点座標の定義
	XMFLOAT3 vertices[] =
	{
		{-1.0f, -1.0f, 0.0f},
		{-1.0f, 1.0f, 0.0f},
		{1.0f, -1.0f, 0.0f}
	};
	//頂点をGPUに伝えるには、GPUにバッファー領域を作成し、コピーする必要がある
	//ID3D12Resourceを使用する -> CPUのnew(malloc())のようなもの
	//ID3D12ResourceはCreateCommittedResource()で作成できる

	//頂点ヒープの設定(ヒープ設定)
	//typedef struct D3D12_HEAP_PROPERTIES {
	//	D3D12_HEAP_TYPE         Type;		//ヒープの種類
	//	D3D12_CPU_PAGE_PROPERTY CPUPageProperty;			//ヒープのCPUページタイプ
	//	D3D12_MEMORY_POOL       MemoryPoolPreference;		//ヒープのメモリプールタイプ
	//	UINT                    CreationNodeMask;			//マルチアダプター操作の場合、これは、リソースを作成する必要があるノード(今回は単一アダプターなので0)
	//	UINT                    VisibleNodeMask;			//マルチアダプター操作の場合、これはリソースが表示されるノードのセット(今回は単一アダプターなので0)
	//} D3D12_HEAP_PROPERTIES;
	D3D12_HEAP_PROPERTIES heapProp = {};
	//typedef enum D3D12_HEAP_TYPE {		//ヒープ種別
	//	D3D12_HEAP_TYPE_DEFAULT,		//CPUからアクセスできない(マップできない->ID3D12Resource::Map()でヒープにアクセスしようとすると失敗する). マップできない代わりにアクセスが早く、GPUのみで扱うヒープとして優秀
	//	D3D12_HEAP_TYPE_UPLOAD,			//CPUからアクセスできる(マップできる->作成したヒープにCPUからアクセスでき、ID3D12Resource::Map()で中身を書き換えられる)CPUのデータをGPUにアップロードするのに使用
	//ただ、D3D12_HEAP_TYPE_DEFAULTのヒープと比べると遅いため、書き込み、読み込み回数を制限したほうが良い
	//	D3D12_HEAP_TYPE_READBACK,		//CPUから読み取れる(呼び戻し専用ヒープ. バンド幅は広くなく(つまり、アクセスは早くない)、GPUで加工・計算したデータをCPUで活用する時に使用. 読み取り専用ということ)
	//	D3D12_HEAP_TYPE_CUSTOM			//カスタムヒープ
	// D3D12_HEAP_TYPE_CUSTOM以外を選択した場合はページリンク設定とメモリープールにUNKNOWNを指定すればよい
	//};
	//マップとは? -> バッファーをCPUのメモリアクセスのように扱うための処理のこと
	heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
	// ヒープのCPUページタイプ
	//typedef enum D3D12_CPU_PAGE_PROPERTY {
	//	D3D12_CPU_PAGE_PROPERTY_UNKNOWN,		//ヒープTypeがD3D12_HEAP_TYPE_CUSTOM以外の時に指定. CUSTOMの時にこれを指定することはできない
	//	D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE,		//CPUからのアクセス不可(GPU内の計算のみで使用するということ. D3D12_HEAP_TYPE_DEFAULTの設定に近い)
	//	D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE,		//CPU -> CPUのメモリ(もしくはキャッシュメモリ)に転送  -> 『時間が出来たら』GPUに転送する(ライトバック方式)方式
	//	D3D12_CPU_PAGE_PROPERTY_WRITE_BACK		// データをある程度まとめて送る方式. 転送順序は考慮されないため注意
	//};
	//typedef enum D3D12_MEMORY_POOL {
	//	D3D12_MEMORY_POOL_UNKNOWN,		//CUSTOM以外の時はこれ
	//	D3D12_MEMORY_POOL_L0,			//システムメモリ(CPUメモリ. ディスクリートGPUではCPUバンド幅は広く、GPUバンド幅は狭くなる). UMAの時はこの選択肢のみ. 
	//	D3D12_MEMORY_POOL_L1			//ビデオメモリ(GPUバンド幅は広くなるが、CPU側からアクセスできなくなる). 統合型グラフィックス(UMA)では使用できない
	//};
	heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProp.CreationNodeMask = 0;
	heapProp.VisibleNodeMask = 0;


	//リソース設定構造体
	//typedef struct D3D12_RESOURCE_DESC {
	//	D3D12_RESOURCE_DIMENSION Dimension;		//使用されているリソースのタイプの設定
	//　バッファーに使用するため D3D12_RESOURCE_DIMENSION_BUFFER を指定
	//	UINT64                   Alignment;		//配置の指定
	//	UINT64                   Width;		//幅ですべてを賄うためsizeof(全頂点)に設定. 画像なら幅を指定
	//	UINT                     Height;		//幅で全て賄うため1. 画像なら高さを指定
	//	UINT16                   DepthOrArraySize;		//リソースが3Dの場合はリソースの深さ、1Dまたは2Dリソースの配列の場合は配列サイズ. 今回は画像ではないため1でよいっぽい
	//	UINT16                   MipLevels;		//ミップレベルの指定
	//	DXGI_FORMAT              Format;		//リソースデータ形式の指定(RGBAとか). 画像じゃないのでUNKNWON指定
	//	DXGI_SAMPLE_DESC         SampleDesc;	//リソースのマルチサンプリングパラメータについての指定(アンチエイリアシングを使用する時はここの値を変えていく)
		//typedef struct DXGI_SAMPLE_DESC {
		//	UINT Count;				//マルチサンプル数の数(0にしてしまうとデータがないことになるので1を指定)
		//	UINT Quality;			//質のレベル(デフォルト0だと思う)
		//} DXGI_SAMPLE_DESC;
	//	D3D12_TEXTURE_LAYOUT     Layout;		//テクスチャレイアウトオプションの指定. テクスチャではないためUNKNOWNが指定できない。今回は最初から最後までメモリが連続しているためD3D12_TEXTURE_LAYOUT_ROW_MAJORを指定
	// テクセルでここを指定する場合は、D3D12_TEXTURE_LAYOUT_ROW_MAJORを指定すると次の行のテクセルの前にメモリ内の行の連続したテクセルを連続して配置するらしい
	//	D3D12_RESOURCE_FLAGS     Flags;		//リソースを操作するためのオプションフラグ(何もしない場合はD3D12_RESOURCE_FLAG_NONE)
	//} D3D12_RESOURCE_DESC;
	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Alignment = 0;
	resourceDesc.Width = sizeof(vertices);
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;		//mipをしようしないので最低の1で
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	//ID3D12Resourceの作成
	ID3D12Resource* vertBuff = nullptr;
	result = _dev->CreateCommittedResource(
		&heapProp,		//ヒープ設定構造体
		D3D12_HEAP_FLAG_NONE,		//ヒープのオプション(特に何もしないのでD3D12_HEAP_FLAG_NONE). ヒープを共有したかったり、バッファー含めたくないなどあればここで指定
		&resourceDesc,		//リソース設定構造体
		D3D12_RESOURCE_STATE_GENERIC_READ,		//リソースの使用方法に関するリソース状態の設定(今回はGPUから読み取り専用にしたいのでD3D12_RESOURCE_STATE_GENERIC_READ)
		nullptr,		//クリアカラーのデフォルト値設定(今回はTextureではないため使用しないのでnullptr)
		IID_PPV_ARGS(&vertBuff)
	);

	//すぐに終了しないようゲームループの作成
	MSG msg = {};

	while (true)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		//アプリケーションを終わらすときはmessageがWM_QUITになる
		if (msg.message == WM_QUIT)
		{
			break;
		}

		/////////////////////////////
		// これより下がDirectX12の処理
		/////////////////////////////

		/////////////////////////////
		// スワップチェーンの動作
		/////////////////////////////

		/////////////////////////////
		// 1. コマンドアロケーターのリセット
		/////////////////////////////

		//result = _commandAllocator->Reset();
		//ここにReset()するコードが有ると、「アロケーターはコマンドリストが記録されているためリセットすることが出来ない」と表示されてしまうので消しておく

		//このあとここに、命令オブジェクトを貯めていく

		/////////////////////////////
		// 2.レンダーターゲットの設定(バックバファーを設定する)
		/////////////////////////////

		//現在のバックバッファーのindex取得(このためにはIDXGISwapChain3以上でswapChainを代入する必要がある)
		//今回はIDXGISwapChain4でスワップチェーンを保存している
		auto backBufferIndex = _swapchain->GetCurrentBackBufferIndex();
		//現在のバックバッファーのindexを保存したので、この値は次のフレームで描画されることになる

		/////////////////////////////
		// バリアの設定
		/////////////////////////////

		D3D12_RESOURCE_BARRIER BarrierDesc = {};
		BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;		//遷移バリア
		BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;			//分割バリアを行うわけではないのでNONE
		BarrierDesc.Transition.pResource = _backBuffers[backBufferIndex];		//バックバッファーのリソースについて説明をすることをGPUに伝える
		BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;				//1つのバックバッファーのみバリアの指定をするので0

		// PRESENT状態が終わるまでレンダーターゲットが待機。バリア実行後はレンダーターゲットとして使用中となる
		//PRESENT = D3D12_RESOURCE_STATE_COMMONと同義
		BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;			//直前はPRESENT状態
		BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;		//今からレンダーターゲット状態にするよ

		//バックバッファを「レンダーターゲットとして使用する」ことをGPUに知らせる
		_commandList->ResourceBarrier(
			1,		//バックバッファー1つだけなので1を指定
			&BarrierDesc
		);

		// レンダーターゲットの指定

		//index0のバックバッファーのポインターを取得
		D3D12_CPU_DESCRIPTOR_HANDLE renderTargetViewHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
		//レンダーターゲットビューとして指定したいバックバッファーのポインタを設定
		renderTargetViewHandle.ptr += backBufferIndex * _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		//コマンドリストが呼んでいる関数-> コマンドリストを貯めている
		_commandList->OMSetRenderTargets(1,
			&renderTargetViewHandle,
			false,		//2つのレンダーターゲットビューは配列のように並んでいるためtrueにする
			nullptr);


		/////////////////////////////
		// 3.レンダーターゲットを指定色でクリア
		/////////////////////////////

		//レンダーターゲットを特定の色でクリア
		float clearColor[] = { 1.0f, 1.0f, 0.0f, 1.0f };		//黄色
		/*
		レンダーターゲットのすべての要素を1つの値に設定
		void ClearRenderTargetView(
		  D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView,		//クリアするレンダーターゲットハンドル
		  const FLOAT [4]             ColorRGBA,			//レンダーターゲットを塗りつぶす色RGBA配列
		  UINT                        NumRects,				//pReactsが指定する配列内の長方形の数
		  const D3D12_RECT            *pRects				//クリアする矩形(D3D12_RECT 構造体)の配列. nullptrを指定した場合(設定したバックバッファの)リソースビュー全体
		);
		*/
		//コマンドリストに貯めていく
		_commandList->ClearRenderTargetView(renderTargetViewHandle, clearColor, 0, nullptr);

		BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;			//直前はPRESENT状態
		BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;		//今からレンダーターゲット状態にするよ


		//バックバッファを「レンダーターゲットとして使用する」ことをGPUに知らせる
		//void ResourceBarrier(
		//	UINT                         NumBarriers,		//設定するバリア説明(D3D12_RESOURCE_BARRIER)の数
		//	const D3D12_RESOURCE_BARRIER* pBarriers		//設定するバリア説明構造体(配列)のアドレス => 配列として複数のバリアをまとめて設定できる
		//);
		_commandList->ResourceBarrier(
			1,		//バックバッファー1つだけなので1を指定
			&BarrierDesc
		);

		/////////////////////////////
		// 4. 貯めておいた命令の実行
		/////////////////////////////

		// 注意; 実行前にコマンドリストはクローズしなければならない

		//コマンドのクローズ
		_commandList->Close();

		//コマンドの実行
		ID3D12CommandList* commandList[] = { _commandList };		//ID3D12GraphicsCommandListはID3D12CommandListインターフェースを継承している
		//コマンドキューでコマンドの実行(コマンド配列の送信)
		_commandQueue->ExecuteCommandLists(
			1,
			commandList
		);

		//フェンスは内部にカウンタを持っており、GetCompletedValue()で取得できる
		//Signal()で設定ができる

		//コマンド実行後、GPUが命令をすべて完了するまで待つ
		//ID3D12CommandQueue::Signal()を呼ぶと、
		//以前にサブミット(つまり現在のループ中に入れられた)されたコマンドがGPU上での実行を完了している、あるいは完了し終わったとき、自動的に第1引数で指定したFenceの値を第2引数の値に更新します
		//ドキュメントではfencenのこの値を" fence value "と呼び、原則 前の値より1大きい値を指定する
		_commandQueue->Signal(_fence, ++_fenceVal);

		//ビジーループを使用しない方法
		//WindowのイベントとWaitForSingleObject()を用いる方法がある

		//すでにGPUの処理を終えていたらイベントの設定はしないでよいので、チェックする
		if (_fence->GetCompletedValue() != _fenceVal)
		{
			//イベントハンドルの取得
			auto event = CreateEvent(
				nullptr,		//セキュリティ記述子 (NULLを指定するとデフォルト値が入る)
				false,			// TRUEなら手動リセットオブジェクトが、FALSEなら自動リセットオブジェクトが作成される。
				//手動リセットオブジェクトはResetEvent関数をCALLしてイベントオブジェクトを非シグナル状態にするもので、自動リセットオブジェクトはWaitForSingleObject関数をCALLした後自動的に非シグナル状態になる。
				//シグナル状態 .. イベントが使用可能状態, 非シグナル .. イベントが使用不可能状態
				false,			//シグナルの初期状態を記述. TRUEならシグナル状態, falseなら非シグナル状態 
				nullptr			//イベントオブジェクトの名前
			);

			_fence->SetEventOnCompletion(
				_fenceVal,
				event
			);

			//指定されたイベントオブジェクトがシグナル状態になるか、指定された時間が経過するまでスレッドを待機
			WaitForSingleObject(event,//同期オブジェクトのハンドル
				INFINITE		//イベントオブジェクトの状態がシグナル状態になるまでの待機する時間.指定した時間内にシグナル状態にならなければ、WaitForSingleObject関数は呼び出し元に制御を返す
			);

			//イベントハンドルを閉じる
			CloseHandle(event);
		}

		//アロケーター内のキューをクリア
		_commandAllocator->Reset();
		//コマンドリストにまた貯められるよう準備(クローズ解除とか)
		_commandList->Reset(_commandAllocator, nullptr);


		/////////////////////////////
		// 5. 画面のスワップ
		/////////////////////////////
		//画面のスワップ(フリップ)を行う
		_swapchain->Present(1, 0);
	}

	//このクラスは使わないので登録解除
	UnregisterClass(w.lpszClassName, w.hInstance);

	return 0;
}