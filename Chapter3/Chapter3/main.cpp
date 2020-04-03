#include <Windows.h>
#include <tchar.h>
#include <d3d12.h>		//DX12の3D用ライブラリ
#include <dxgi1_6.h>		//DXGI(Direct3D APIよりもドライバーに近いディスプレイ出力に直接関係する機能を制御するためのAPIセット)
#include <vector>
//DXGIはハードウェアやドライバに近いため、Direct3Dを介して操作してほしいが、
//ディスプレイの列挙(全ディスプレイの列挙)や画面フリップ(画面を弾いたかどうか)はユーザーがDXGIに直接命令しないとならない

// 必要なlibファイルの追加(timeGetTimeに必要)
//#pragma commentについて
//オブジェクトファイルに、リンカでリンクするライブラリの名前を記述するもの。
//#pragma comment(lib, "path")のコードでいう、pathsというライブラリをリンカでリンクするということ
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

#ifdef  _DEBUG
#include <iostream>
#endif //  _DEBUG
using namespace std;

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
ID3D12CommandList* _commandList = nullptr;
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
IDXGISwapChain* _swapchain = nullptr;

//ウィンドウアプリケーションはマウス操作やキーボードなどプレイヤーの発生させたイベントを契機にして、次の処理を行う(イベント駆動型プログラミング)
//イベント駆動型プログラミングではイベントを処理する関数が用意される(イベントハンドラ)
//Windowsアプリケーションにおけるイベントハンドラがウィンドウプロシージャ(window procedure)
//イベントが発生すると、messageとしてアプリケーションに通知
/*
hwnd: メッセージを受け取るウィンドウプロシージャを持つウィンドウへのハンドルです。
message: WM_DESTROYやWM_MOVEといった、メッセージコード
wParam, lParam には，キーボードのキーコード，マウスカーソルの座標など，メッセージの種類に応じた付加情報が格納される
(メッセージごとに決まっている)

返却型 LRESULT について0: ウィンドウプロシージャの返り値の意味はメッセージによって異なりますが，
返り値が特別な意味を持たないメッセージでは 0 を返すのが普通
*/
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
	//_T("")マクロは、ユニコードとマルチバイトの差異を解消するためのマクロ
	//例えば _T("ABC")とプログラム中に書いた場合、Unicodeが定義されたシステム内では、L"ABC"と書かれたのと同じ16ビットのワイド文字型になり、
	//Unicodeが定義されていなければ、ただの"ABC"で書かれたのと同じ、8ビットのANSI文字列になる
	//C++ではビルド設定によって使用する文字セットをANSIにするかUnicodeにするかを切り替えることができるため、文字列は_T("")で書くほうが良い
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
	//WS_OVERLAPPEDWINDOW = WS_OVERLAPPED(タイトルバー，境界線つきオーバーラップウインドウ) 
	// | WS_CAPTION	タイトルバー(WS_DLGFRAME[サイズ変更できない境界線]も一緒についてくる)
	// | WS_SYSMENU タイトルバーにシステムメニュー(閉じるボタンとアイコン)つける
	// | WS_THICKFRAME サイズ変更できる境界線
	// | WS_MINIMIZEBOX システムメニューに最小化ボタン加える
	// | WS_MAXIMIMZEBOX システムメニューに最大化ボタン加える
	//3つめの引数はメニューフラグ
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

	//IDXGIFactory6 :: EnumAdapterByGpuPreference(): 指定されたGPU設定に基づいてグラフィックアダプターを列挙します。
	//上の関数を使用するために、IDXGIFactory6型のハードやドライバーの情報が入る構造体の取得を行う
	//CreateDXGIFactory2: 他のDXGIオブジェクトの生成に使用できるDXGI 1.3ファクトリを作成します。
	//CreateDXGIFactory1: 他のDXGIオブジェクトの生成に使用できるDXGI 1.1ファクトリを作成します。
	//Version的にCreateDXGIFactory2を使ったほうが良さそう. 
	/*
	HRESULT CreateDXGIFactory2(
	  UINT   Flags,
	  REFIID riid,
	  void   **ppFactory
	);
	Flags: UINT: 0かDXGI_CREATE_FACTORY_DEBUGが入る
	DXGI_CREATE_FACTORY_DEBUG .. DirectX12ではCreateDXGIFactory2()を使うとき、DXGI_CREATE_FACTORY_DEBUGフラグを使用しないとならない(らしい)
	riid, ppFactory: 戻り値はIID_PPV_ARGSを使用
	返り値に使用しているドライバーやハードの情報の入った変数_dxgiFactoryが返ってくる
	*/
	if (FAILED(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&_dxgiFactory))))
	{
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

		//DXGI_ADAPTER_DESCのDescriptionについて
		//グラフィックハードウェアのLvが8以下: アダプタの説明を含む文字列
		//グラフィックハードウェアのLvが9以上: GetDesc()は説明文字列に対して「ソフトウェアアダプター」を返す
		//wstringはwchar_t型文字列. wchar_tは16ビットの環境で、UTF-16の文字列として使用	
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
			IID_PPV_ARGS(&_dev))		//受け取りたいオブジェクトの型を識別するID(多分デバイスのオブジェクトの型を識別するID) および デバイスの実体のポインターを指定する必要があるが、
			//IID_PPV_ARGSマクロにの引数にデバイスオブジェクトを代入すると、 REFIID(受け取りたいオブジェクトの型指定)とデバイスの実体のポインターのアドレスに解釈される(1つの引数で済む)
			//多分だが、Dx12では受け取り系はIID_PPV_ARGSを使うのかも(使わなくてもいいけど、面倒)
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
	/*
	CreateCommandAllocator関数
	D3D12_COMMAND_LIST_TYPE_DIRECT: GPUが実行できるコマンドバッファーを指定

	第1引数：コマンドアロケータの種類
	第2引数：各インタフェース固有のGUID
	第3引数：ID3D12CommandAllocatorインタフェースのポインタを格納する変数のアドレス

	第2、第3引数は、IID_PPV_ARGSマクロを使うことで簡易的に受け渡しが可能。
	*/
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

	/*
	typedef struct DXGI_SWAP_CHAIN_DESC1 {
	  UINT             Width;		//画面幅
	  UINT             Height;		//画面高さ
	  DXGI_FORMAT      Format;		//ピクセルフォーマット
	  BOOL             Stereo;		//立体視を使用するか有無を指定
	  DXGI_SAMPLE_DESC SampleDesc;	//サンプリング回数や品質の設定
		typedef struct DXGI_SAMPLE_DESC {
		  UINT Count;		//サンプリング回数. アンチエイリアスを使用しない場合は1指定
		  UINT Quality;		//サンプリング品質. アンチエイリアスを使用しない場合は0指定
		} DXGI_SAMPLE_DESC;
	  DXGI_USAGE       BufferUsage;		//フレームバッファの使用方法
	   -> DXGI_USAGE_BACK_BUFFER: サーフェスまたはリソースはバックバッファーとして使用
	  UINT             BufferCount;		//バッファの数. ダブルバッファなら2
	  DXGI_SCALING     Scaling;		//フレームバッファとウィンドウのサイズが異るときにどう表示するか
		typedef enum DXGI_SCALING {
		  DXGI_SCALING_STRETCH,		//ウィンドウのサイズに引き伸ばし
		  DXGI_SCALING_NONE,		//引き伸ばしをしない
		  DXGI_SCALING_ASPECT_RATIO_STRETCH		//縦横比の比率を維持して引き伸ばす
		} ;
	  DXGI_SWAP_EFFECT SwapEffect;		//ディスプレイにフレーム表示後の表画面のピクセルをどうするか
		typedef enum DXGI_SWAP_EFFECT {
		  DXGI_SWAP_EFFECT_DISCARD,				//DXGIがバックバッファーの内容を破棄(複数のバックバッファーを持つスワップチェーンに対して有効)
		  DXGI_SWAP_EFFECT_SEQUENTIAL,			//DXGIがバックバッファーのコンテンツを保持
		  DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL,		//DXGIがバックバッファーのコンテンツを保持するように指定(WPF: ウィンドウズデスクトップアニメーションで使用する?)
		  DXGI_SWAP_EFFECT_FLIP_DISCARD			//DXGIがバックバッファーの内容を破棄(WPF: ウィンドウズデスクトップアニメーションで使用する?)
		} ;
	  DXGI_ALPHA_MODE  AlphaMode;		//フレームバッファをウィンドウに表示する際の「アルファ値の扱い方」
	  typedef enum DXGI_ALPHA_MODE {
		  DXGI_ALPHA_MODE_UNSPECIFIED,		//動作指定なし(GPUに任せる)
		  DXGI_ALPHA_MODE_PREMULTIPLIED,	//アルファ値はあらかじめ乗算されているものである
		  DXGI_ALPHA_MODE_STRAIGHT,			//アルファ値は乗算されていない. つまり、カラーの透明度そのままの値
		  DXGI_ALPHA_MODE_IGNORE,			//アルファ値は無視
		  DXGI_ALPHA_MODE_FORCE_DWORD		//この列挙体を強制的に32ビットサイズにコンパイル(通常使わないと思われる)
		} ;
	  UINT             Flags;		//DXGI_SWAP_CHAIN_FLAGの値をOR演算子で組み合わせたものが入る. スワップチェーンの動作オプションを指定したものが入る
	} DXGI_SWAP_CHAIN_DESC1;
	*/
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
	/*
	typedef struct D3D12_DESCRIPTOR_HEAP_DESC {
	  D3D12_DESCRIPTOR_HEAP_TYPE  Type;		//なんのビューを作るか
		typedef enum D3D12_DESCRIPTOR_HEAP_TYPE {
		  D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,		// CBV(定数バッファ), SBV(テクスチャバッファー), UAV(コンピュートシェーダ)
		  D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,			// サンプラー
		  D3D12_DESCRIPTOR_HEAP_TYPE_RTV,				// RTV(レンダータッゲットビュー)
		  D3D12_DESCRIPTOR_HEAP_TYPE_DSV,				// DSV(深度ステンシルビュー)
		  D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES			// 記述子ヒープのタイプの数(?)
		} ;
	  UINT                        NumDescriptors;	//格納できるディスクリプタの数(デスクリプタヒープが配列の為、配列数の指定が必要)
	  D3D12_DESCRIPTOR_HEAP_FLAGS Flags;		//ディスクリプタのビューの情報をシェーダー側が参照する必要があるか
	  //SRV(テクスチャバッファー)やCBV(定数バッファー)ならD3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLEだが、今回は教える必要はないのでD3D12_DESCRIPTOR_HEAP_FLAG_NONE
	  UINT                        NodeMask;		//複数のGPUを使用する場合、どのGPU向けのデスクリプタヒープかを指定。GPUを1つしか使用しない場合、0を設定
	} D3D12_DESCRIPTOR_HEAP_DESC;
	*/
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

	/*
	CreateDescriptorHeap関数
		第1引数：D3D12_DESCRIPTOR_HEAP_DESC構造体へのポインタ
		第2引数：各インタフェースの固有GUID
		第3引数：ID3D12DescriptorHeapインタフェースのポインタを格納する変数のアドレス

		GUIDやインタフェースを格納した変数のアドレスに関しては、IID_PPV_ARGSマクロを使用することが可能
	*/

	result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeap));

	/////////////////////////////
	// ディスクリプタヒープ内のディスクリプタとバックバッファーの関連付け
	/////////////////////////////

	//スワップチェーンのバックバッファーの数を取得したいために、スワップチェーンの説明を取得する
	//(なお、これはディスクリプタヒープの配列数とバックバッファーの数を知りたいためにやっているので、ディスクリプタヒープの配列数から算出しても良い)
	DXGI_SWAP_CHAIN_DESC swapChaDesc = {};
	result = _swapchain->GetDesc(&swapChaDesc);			//この関数使うよりはIDXGISwapChain1::GetSwap1()を使ってねとのこと
	
	//ID3D12Resource: CPUとGPUの物理メモリまたはヒープへの読み書きの一般化されたできることをカプセル化したもの. 
	//GPUメモリにある、様々なデータを管理するための汎用クラス
	//配列、テクスチャ、モデルといったデータは、全てID3D12Resouceインターフェイスを通して管理
	//この変数にはスワップチェーンのバックバッファーのメモリを入れていく
	vector<ID3D12Resource*> _backBuffers(swapChaDesc.BufferCount);		//vector<T> t(n);		でT型の変数tを配列要素nで確保ということになる
	for (int idx = 0; idx < swapChaDesc.BufferCount; idx++)
	{
		//スワップチェーン内のバッファーとビューの関連付け

		//スワップチェーン内のバックバッファーのメモリの取得
		result = _swapchain->GetBuffer(idx, IID_PPV_ARGS(&_backBuffers[idx]));

		//(スワップチェーン内の)バックバッファーとビュー(ディスクリプタ)の関連付け
		//デスクリプタヒープ上にRTV用のデスクリプタを作成する関数
		/*
		void CreateRenderTargetView(
		  ID3D12Resource                      *pResource,		//(RTVの)レンダーターゲット(描画先)を表すポインター
		  const D3D12_RENDER_TARGET_VIEW_DESC *pDesc,			//作成するビューの設定内容
		  //nullptrに設定するとミップマップの0番のサブリソースにアクセスできるらしいが、RTVはミップマップ関係ないのでnullptr
		  D3D12_CPU_DESCRIPTOR_HANDLE         DestDescriptor	//ディスクリプタヒープにおけるビューのアドレスが入る
		  //D3D12_CPU_DESCRIPTOR_HANDLE: ディスクリプタヒープにおけるアドレスのようなもの
		  typedef struct D3D12_CPU_DESCRIPTOR_HANDLE {
			  SIZE_T ptr;		//ここにアドレスが入ってくる
		  } D3D12_CPU_DESCRIPTOR_HANDLE;
		);
		*/
		//ディスクリプタヒープの先頭のアドレスを受け取る
		D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeap->GetCPUDescriptorHandleForHeapStart();

		//ID3D12Device::GetDescriptorHandleIncrementSize(): ディスクリプタ1つ1つのサイズを返す
		//引数にはディスクリプタの種類(D3D12_DESCRIPTOR_HEAP_TYPE)を指定(レンダーターゲットビューはD3D12_DESCRIPTOR_HEAP_TYPE_RTV)
		handle.ptr += idx * _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		_dev->CreateRenderTargetView(
			_backBuffers[idx],
			nullptr,
			//ディスクリプタヒープハンドルを取得するには ID3D12DescriptaHeap::GetCPUDescriptorHandleForHeapStart()
			//ID3D12DescriptaHeap::GetCPUDescriptorHandleForHeapStart()はディスクリプタヒープの「先頭の」アドレスしか取得できない
			//1. ビューの種類によってディスクリプタが必要とするサイズが異なる(ずらすバイトが異なる)
			//2. 受け渡し時に使用するのはハンドルであってアドレスそのものではない
			//そのため、「ポインター + i」とできない
			//rtvHeap->GetCPUDescriptorHandleForHeapStart()
			handle
		);
	}

	//ウィンドウの表示
	ShowWindow(hwnd, SW_SHOW);

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
	}

	//このクラスは使わないので登録解除
	UnregisterClass(w.lpszClassName, w.hInstance);

	return 0;
}