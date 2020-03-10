#include <Windows.h>
#include <tchar.h>
#include<d3d12.h>		//DX12の3D用ライブラリ
#include<dxgi1_6.h>		//DXGI(Direct3D APIよりもドライバーに近いディスプレイ出力に直接関係する機能を制御するためのAPIセット)
//DXGIはハードウェアやドライバに近いため、Direct3Dを介して操作してほしいが、
//ディスプレイの列挙(全ディスプレイの列挙)や画面フリップ(画面を弾いたかどうか)はユーザーがDXGIに直接命令しないとならない

// 必要なlibファイルの追加(timeGetTimeに必要)
//#pragma commentについて
//オブジェクトファイルに、リンカでリンクするライブラリの名前を記述するもの。
//#pragma comment(lib, "path")のコードでいう、pathsというライブラリをリンカでリンクするということ
#pragma comment(lib, "d3d12.h")
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

//基本オブジェクトの変数
ID3D12Device* _dev = nullptr;		//デバイスオブジェクト
IDXGIFactory* _dgiFactory = nullptr;
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
		_T("DX12テスト"),		//タイトルバーの文字
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

	//DX12周りの初期化

	//フィーチャーレベルの列挙(選択されたグラフィックドライバーが対応していないフィーチャーレベルだったら下げていくために)
	D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};

	//DX3Dのデバイス初期化

	D3D_FEATURE_LEVEL featureLevel;		//対応しているフィーチャーレベルのうち、最も良かったものが代入される
	for (auto level : levels)
	{
		//デバイスオブジェクトの作成関数(成功時はS_OKを返す)
		if (D3D12CreateDevice(nullptr,		//グラフィックドライバーのアダプターのポインタ(nullptrにすると自動で選択される)
			//複数のグラフィックドライバーが刺さっている場合は自動で選ばれたアダプターが最適なものとは限らないので注意

			level,			//最低限必要なフィーチャーレベル
			IID_PPV_ARGS(&_dev))		//受け取りたいオブジェクトの型を識別するID(多分デバイスのオブジェクトの型を識別するID) および デバイスの実体のポインターを指定する必要があるが、
			//IID_PPV_ARGSマクロにの引数にデバイスオブジェクトを代入すると、 REFIID(受け取りたいオブジェクトの型指定)とデバイスの実体のポインターのアドレスに解釈される(1つの引数で済む)
			== S_OK)
		{
			featureLevel = level;
			break;
		}
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