#include <Windows.h>
#include <tchar.h>
#include <d3d12.h>		//DX12��3D�p���C�u����
#include <dxgi1_6.h>		//DXGI(Direct3D API�����h���C�o�[�ɋ߂��f�B�X�v���C�o�͂ɒ��ڊ֌W����@�\�𐧌䂷�邽�߂�API�Z�b�g)
#include <vector>
#include <DirectXMath.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

#ifdef  _DEBUG
#include <iostream>
#endif //  _DEBUG

using namespace std;
using namespace DirectX;

// @brief: �R���\�[����ʂɃt�H�[�}�b�g�t���������\��
// @param format �t�H�[�}�b�g (%f, %d�Ȃ�)
// @param �ϒ�����
// @remaks ���̊֐��̓f�o�b�O�p.�f�o�b�O���̂ݓ���
void DebugOutputFormatString(const char* format, ...)
{
	// DebugOutputFormatString("%d %f %s\n", 100, 3.14, "piyo");�̂悤�Ɏg����悤�ɂ���
#ifdef _DEBUG
	va_list valist;
	va_start(valist, format);		//�ϒ�����(��Ō��� 100, 3.14, "piyo")��1�̕ϐ�(valist)�ɂ܂Ƃ߂�
	printf(format, valist);
	va_end(valist);
#endif
}

const unsigned int window_width = 1280;
const unsigned int window_height = 720;

//�R�}���h���X�g�̎Q��(GPU�ɑ΂��閽�߂��܂Ƃ߂邽�߂̃I�u�W�F�N�g)
ID3D12GraphicsCommandList* _commandList = nullptr;
//�R�}���h�A���P�[�^�[�̎Q��(�R�}���h���X�g���g�p����̂ɕK�v)
//�R�}���h���X�g�Ɋi�[���閽�߂ׂ̈̃��������Ǘ�����I�u�W�F�N�g
ID3D12CommandAllocator* _commandAllocator = nullptr;
//�R�}���h�L���[�̎Q��
ID3D12CommandQueue* _commandQueue = nullptr;

//��{�I�u�W�F�N�g�̕ϐ�
ID3D12Device* _dev = nullptr;		//�f�o�C�X�I�u�W�F�N�g(�f�o�C�X�Ɋւ���Q��)
//IDXGIFactory6: �����GPU�ݒ�Ɋ�Â��ăO���t�B�b�N�A�_�v�^�[��񋓂���P��̃��\�b�h���L���ɂȂ�C���^�[�t�F�[�X
//���̃C���^�[�t�F�[�X�� 
// IDXGIFactory6 :: EnumAdapterByGpuPreference(): �w�肳�ꂽGPU�ݒ�Ɋ�Â��ăO���t�B�b�N�A�_�v�^�[��񋓂��܂��B
// �Ƃ����֐�����������
IDXGIFactory6* _dxgiFactory = nullptr;
IDXGISwapChain4* _swapchain = nullptr;

//�E�B���h�E�A�v���P�[�V�����̓}�E�X�����L�[�{�[�h�Ȃǃv���C���[�̔����������C�x���g���_�@�ɂ��āA���̏������s��(�C�x���g�쓮�^�v���O���~���O)
LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	//�E�B���h�E���j������鎞�ɔ���
	if (msg == WM_DESTROY)
	{
		//OS�ɑ΂��āA�A�v�����I�����邱�Ƃ�`����
		PostQuitMessage(0);
		return 0;
	}
	//�K��̏������s��(�����̂Ȃ����b�Z�[�W�̏����́C����̃E�B���h�E�v���V�[�W���ł��� DefWindowProc �Ɋۓ���)
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

///�f�o�b�O���C���[��L����
void EnableDebugLayer()
{
	//ID3D12Debug: �f�o�b�O�ݒ��������A�p�C�v���C���X�e�[�g�̌��؂��s�����߂̃C���^�t�F�[�X
	ID3D12Debug* debugLayer = nullptr;
	//D3D12GetDebugInterface: �f�o�b�N�C���^�[�t�F�[�X���擾
	auto result = D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer));
	//EnableDebugLayer(): �f�o�b�O���C���[�̗L��
	debugLayer->EnableDebugLayer();		//�f�o�b�O���C���[�̗L����
	debugLayer->Release();		//�L���������̂ŁA�C���^�[�t�F�[�X��j��
}

#if _DEBUG		//�f�o�b�O���[�h(�ŏ��̕��̓R�}���h���C���o�͂ŏ��\�����邽��)
int main()
{
#else // _DEBUG
#include<Windows.h>		//���ꂪ�Ȃ��Ɖ��̂� _T("DX12�T���v��"), �œ��{����g���ƃG���[�ɂȂ�(��ł���`���Ă���񂾂��ǂ�....)
// Windows�v���O�����̎�v�ȃC���N���[�h�t�@�C����WINDOWS.H
// Windows�v���O������main()�֐��͖���. �G���g���[�|�C���g��WinMain()
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)		//��f�o�b�O���[�h
{
#endif
	DebugOutputFormatString("Show windows test");

	//WNDCLASSEX �\���̂ŃE�C���h�E�N���X�̃p�����^���w��
	//RegisterClassEx()�� ���̃E�C���h�E�N���X��o�^
	//CreateWindow()�œo�^�����N���X�����ɃE�C���h�E�����
	//ShowWindow()�ŕ\������D�Ƃ����̂���A�̗���
	WNDCLASSEX w = {};

	//WNDCLASSEX�̍\���̂̃T�C�Y(UINT)
	w.cbSize = sizeof(WNDCLASSEX);
	//���̃E�C���h�E�̃��b�Z�[�W����������R�[���o�b�N�֐��ւ̃|�C���^(WNDPROC)
	w.lpfnWndProc = (WNDPROC)WindowProcedure;
	//���̃E�C���h�E�N���X�ɂ��閼�O(�K���ŗǂ�)(LPCTSTR)
	w.lpszClassName = _T("DX12Sample");
	//�n���h���̎擾[���̃N���X�̂��߂̃E�C���h�E�v���V�[�W��������C���X�^���X�n���h��](������hinstance�����̂܂ܑ�����Ă���T�C�g���L��)
	w.hInstance = GetModuleHandle(nullptr);

	//�E�C���h�E�N���X�̓o�^(���������̍�邩���낵������OS�ɗ\������)
	RegisterClassEx(&w);

	//�E�B���h�E�T�C�Y�̌���
	RECT wrc = {
		0,		//�l�p�`�̍����x���W�ʒu
		0,		//�l�p�`�̍����y���W�ʒu
		window_width,		//����
		window_height		//�c��
	};

	//�E�B���h�E�̃T�C�Y�␳
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	//�E�B���h�E�̐���
	//�Ԃ�l: �쐬���ꂽ�E�B���h�E�̃n���h��. ���s������NULL
	HWND hwnd = CreateWindow(
		w.lpszClassName,		//RegisterClassEx()�œo�^�����N���X�̖��O(WNDCLASS.lpszClassName)���C��`�ς݂̃N���X
		_T("DX12�T���v��"),		//�^�C�g���o�[�̕���
		WS_OVERLAPPEDWINDOW,		//�E�C���h�E�X�^�C��. WS_OVERLAPPEDWINDOW�Ɋւ��Ă͏�Q�l
		CW_USEDEFAULT,			//�E�C���h�E����x���W(��ʍ��オ0)�D�K���ł�������CW_USEDEFAULT
		CW_USEDEFAULT,			//�E�C���h�E����y���W(��ʍ��オ0)�D�K���ł�������CW_USEDEFAULT
		wrc.right - wrc.left,		//�E�C���h�E���D�K���ł�������CW_USEDEFAULT
		wrc.bottom - wrc.top,		//�E�C���h�E�����D�K���ł�������CW_USEDEFAULT
		nullptr,		//�e�E�C���h�E�̃n���h���D�Ȃ����NULL�D3�߂̈�����DW_CHILD���w�肵���Ƃ��͕K�{�D
		nullptr,		//���j���[�̃n���h���D�f�t�H���g(WNDCLASS.lpszMenuName)���g���ꍇ��NULL�D
		w.hInstance, 	//�E�C���h�E�Ƃ����쐬���郂�W���[��(�Ăяo���A�v���P�[�V�������W���[��)�̃C���X�^���X�̃n���h��
		nullptr			//�ǉ��p�����[�^
	);

#ifdef _DEBUG
	//�f�o�b�O���C���[�L����
	EnableDebugLayer();
#endif // DEBUG


	/////////////////////////////
	// DX12����̏�����
	/////////////////////////////

	//�t�B�[�`���[���x���̗�(�I�����ꂽ�O���t�B�b�N�h���C�o�[���Ή����Ă��Ȃ��t�B�[�`���[���x���������牺���Ă������߂�)
	D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};

	//�O���t�B�b�N�{�[�h�������h�����Ă��邱�Ƃ��l�����āA�A�_�v�^�[��񋓂��Ă���
	if (FAILED(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&_dxgiFactory))))
	{
		//���߂������̂ŁA�ʏ�̃��[�h�ł�DXGI�I�u�W�F�N�g���擾���ɍs��
		//DXGI_CREATE_FACTORY_DEBUG�t���O�ł��߂������Ƃ��悤.�����ADirectX12�Ńt���O��0�ɂ���̂͐�������Ă��Ȃ��C������
		if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&_dxgiFactory))))
		{
			return -1;
		}
	}

	//�O���t�B�b�N�{�[�h�̎g�p�ł���A�_�v�^�[��񋓂��Ă���
	vector<IDXGIAdapter*> adapters;

	//�����ł����A�_�v�^�[�������Ă���
	IDXGIAdapter* tmpAdapter = nullptr;
	//EnumAdapters(): Enumerates the adapters (video cards).�r�f�I�J�[�h�A�_�v�^��񋓂������̂�Ԃ��֐�
	//��������S_OK. ���[�J���V�X�e���Ɏh�����Ă���A�_�v�^�[��葽������index��������DXGI_ERROR_NOT_FOUND, ��2������nullptr�Ȃ�DXGI_ERROR_INVALID_CALL ��Ԃ�(�������HRESULT�^)
	for (int i = 0; _dxgiFactory->EnumAdapters(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; ++i)
	{
		adapters.push_back(tmpAdapter);
	}

	for (auto adapter : adapters)
	{
		//�A�_�v�^�[�̖��O�����Ɏg�p����A�_�v�^�[�����߂Ă���

		//DXGI1.0�܂ł̃A�_�v�^�[���r�f�I�J�[�h�̐����p�\����
		//Describes an adapter (or video card) by using DXGI 1.0.
		DXGI_ADAPTER_DESC  adesc = {};		//������
		adapter->GetDesc(&adesc);		//�A�_�v�^�[������������擾

		std::wstring strDesc = adesc.Description;

		//�T�������A�_�v�^�[�̖��O�Ŋm�F
		/*
		���C�h�����񃊃e����(1����������̃o�C�g����ʏ��葽�������f�[�^�^)�̎��́A
		�����񃊃e�����ɑ΂��� L ������K�v������
		*/
		//std::string::npos: string::find�Œl��������Ȃ������ꍇ�ɕԂ��l�Ƃ��Ē�`����Ă���
		if (strDesc.find(L"NVIDIA") != std::string::npos)		//�\�t�g�E�F�A�A�_�v�^�[���Ɏw�肵�������񂪂���Ȃ�
		{
			//������ꂽ�̂ő��
			tmpAdapter = adapter;
			break;
		}
	}

	/////////////////////////////
	// DX3D�̃f�o�C�X������
	/////////////////////////////
	D3D_FEATURE_LEVEL featureLevel;		//�Ή����Ă���t�B�[�`���[���x���̂����A�ł��ǂ��������̂���������
	for (auto level : levels)
	{
		//�f�o�C�X�I�u�W�F�N�g�̍쐬�֐�(��������S_OK��Ԃ�)
		if (D3D12CreateDevice(tmpAdapter,		//�O���t�B�b�N�h���C�o�[�̃A�_�v�^�[�̃|�C���^(nullptr�ɂ���Ǝ����őI�������)
			//�����̃O���t�B�b�N�h���C�o�[���h�����Ă���ꍇ�͎����őI�΂ꂽ�A�_�v�^�[���œK�Ȃ��̂Ƃ͌���Ȃ��̂Œ���

			level,			//�Œ���K�v�ȃt�B�[�`���[���x��
			IID_PPV_ARGS(&_dev))
			== S_OK)
		{
			featureLevel = level;
			break;
		}
	}

	//�쐬���ʂƂ���������ϐ�
	HRESULT result;

	/////////////////////////////
	// �R�}���h�A���P�[�^�[�̍쐬
	/////////////////////////////
	result = _dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_commandAllocator));

	//�R�}���h���X�g�̍쐬
	result = _dev->CreateCommandList(0, //	�V���O��GPU�I�y���[�V�����̏ꍇ�A������[���ɐݒ�
		D3D12_COMMAND_LIST_TYPE_DIRECT, //�쐬����R�}���h���X�g�̃^�C�v
		_commandAllocator,		//�f�o�C�X���R�}���h���X�g���쐬����R�}���h�A���P�[�^�[�I�u�W�F�N�g�ւ̃|�C���^�[
		nullptr,	// �p�C�v���C����ԃI�u�W�F�N�g�ւ̃I�v�V�����|�C���^�[
		IID_PPV_ARGS(&_commandList)		//�쐬���ꂽ�R�}���h���X�g���Ԃ����ϐ�
	);

	/////////////////////////////
	// �R�}���h�L���[�̎��̂̍쐬
	/////////////////////////////

	//�R�}���h�L���[�̐ݒ�
	//D3D12_COMMAND_QUEUE_DESC�\��
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {};

	//�^�C���A�E�g�Ȃ�
	//(�f�t�H���g�̃R�}���h�L���[)
	commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

	//�A�_�v�^�[��1�����g��Ȃ��̂�0
	/*
	�V���O��GPU����̏ꍇ�A������[���ɐݒ�.
	������GPU�m�[�h������ꍇ�́A�r�b�g��ݒ肵�āA�R�}���h�L���[���K�p�����m�[�h�i�f�o�C�X�̕����A�_�v�^�[�j������
	*/
	commandQueueDesc.NodeMask = 0;

	//�v���C�I���e�B�͎w��Ȃ�
	//�R�}���h�L���[�� �D��x
	commandQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;

	//��ނ̓R�}���h���X�g�Ɠ��l�ɂ���
	commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	//�L���[�쐬
	result = _dev->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&_commandQueue));

	/////////////////////////////
	// �X���b�v�`�F�[���̍쐬
	/////////////////////////////

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width = window_width;
	swapChainDesc.Height = window_height;
	swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;		//RGBA��8byte���g�p����(256bit)
	swapChainDesc.Stereo = false;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
	swapChainDesc.BufferCount = 2;

	//�t���[���o�b�t�@�̓E�B���h�E�̃T�C�Y�Ɉ����L�΂�
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;

	//�t���[���\����̓o�b�t�@�[�̒��g��j��
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	//�A���t�@�l��GPU�ɂ��܂���
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

	//�E�B���h�E <-> �t���X�N���[�����[�h �Őؑ։\�ɐݒ�
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	//�X���b�v�`�F�[���̐���
	result = _dxgiFactory->CreateSwapChainForHwnd(
		_commandQueue,		//�R�}���h�L���[�I�u�W�F�N�g
		hwnd,				//�E�B���h�E�n���h��
		&swapChainDesc,		//�X���b�v�`�F�[���ݒ�
		nullptr,			//DXGI_SWAP_CHAIN_FULLSCREEN_DESC(�X���b�v�`�F�[���̃t���X�N���[��)�\���̂̃|�C���^(�w�肵�Ȃ��ꍇ��nullptr)
		nullptr,			//IDXGIOutput�C���^�t�F�[�X�̃|�C���^(�}���`���j�^�[���g�p����ꍇ�́A�o�͐�̃��j�^�[�w��̂��߂Ɏg�p����. �g�p���Ȃ��Ȃ�nullptr)
		(IDXGISwapChain1**)&_swapchain		//�X���b�v�`�F�[���쐬�������ɂ��̕ϐ��ɃC���^�[�t�F�[�X�̃|�C���^���i�[�����
	);

	//�X���b�v�`�F�[�����쐬�������A�X���b�v�`�F�[���̃o�b�N�o�b�t�@�[���N���A������A�������񂾂�͂��̂܂܂ł͂ł��Ȃ�
	/*�����_�[�^�b�Q�b�g�r���[�Ƃ����r���[���K�v
		��
	  �����_�[�^�[�Q�b�g�r���[(�r���[�܂�f�B�X�N���v�^�̂���)������f�B�X�N���v�^�q�[�v���K�v
	*/

	/////////////////////////////
	// �f�B�X�N���v�^�q�[�v�̍쐬(�r���[�̓f�B�X�N���v�^�q�[�v���Ȃ��ƍ��Ȃ�����)
	/////////////////////////////

	//�f�B�X�N���v�^�q�[�v�̓f�B�X�N���v�^�𕡐�����邽�߂̃q�[�v�̈�

	//�q�[�v���쐬���邽�߂̐ݒ�
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	//����̃f�B�X�N���v�^�̓����_�[�^�[�Q�b�g�r���[�Ƃ��Ďg�p
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	//GPU��1�����g�p���Ȃ��̂�0
	heapDesc.NodeMask = 0;
	//�\��ʂƗ���ʂ�2�̃o�b�t�@�[�ɑ΂���r���[�Ȃ̂�2���w��
	heapDesc.NumDescriptors = 2;
	//�V�F�[�_�[������f�B�X�N���v�^�̃r���[�̏����Q�Ƃ��Ȃ��̂�D3D12_DESCRIPTOR_HEAP_FLAG_NONE���w��
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	ID3D12DescriptorHeap* rtvHeap = nullptr;

	result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeap));

	/////////////////////////////
	// �f�B�X�N���v�^�q�[�v���̃f�B�X�N���v�^�ƃo�b�N�o�b�t�@�[�̊֘A�t��
	/////////////////////////////

	//�X���b�v�`�F�[���̃o�b�N�o�b�t�@�[�̐����擾���������߂ɁA�X���b�v�`�F�[���̐������擾����
	DXGI_SWAP_CHAIN_DESC swapChaDesc = {};
	result = _swapchain->GetDesc(&swapChaDesc);			//���̊֐��g������IDXGISwapChain1::GetSwap1()���g���Ă˂Ƃ̂���

	vector<ID3D12Resource*> _backBuffers(swapChaDesc.BufferCount);		//vector<T> t(n);		��T�^�̕ϐ�t��z��v�fn�Ŋm�ۂƂ������ƂɂȂ�

	//�f�B�X�N���v�^�q�[�v�̐擪�̃A�h���X���󂯎��
	D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeap->GetCPUDescriptorHandleForHeapStart();

	for (int idx = 0; idx < swapChaDesc.BufferCount; idx++)
	{
		//�X���b�v�`�F�[�����̃o�b�t�@�[�ƃr���[�̊֘A�t��

		//�X���b�v�`�F�[�����̃o�b�N�o�b�t�@�[�̃������̎擾
		result = _swapchain->GetBuffer(idx, IID_PPV_ARGS(&_backBuffers[idx]));

		//(�X���b�v�`�F�[������)�o�b�N�o�b�t�@�[�ƃr���[(�f�B�X�N���v�^)�̊֘A�t��
		//�f�X�N���v�^�q�[�v���RTV�p�̃f�X�N���v�^���쐬����֐�
		_dev->CreateRenderTargetView(
			_backBuffers[idx],
			nullptr,
			handle
		);

		//ID3D12Device::GetDescriptorHandleIncrementSize(): �f�B�X�N���v�^1��1�̃T�C�Y��Ԃ�
		//�����ɂ̓f�B�X�N���v�^�̎��(D3D12_DESCRIPTOR_HEAP_TYPE)���w��(�����_�[�^�[�Q�b�g�r���[��D3D12_DESCRIPTOR_HEAP_TYPE_RTV)
		handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	/////////////////////////////
	// �t�F���X�̍쐬(GPU�̏������I����������Ď��������)
	/////////////////////////////
	//�t�F���X�̃C���X�^���X
	ID3D12Fence* _fence = nullptr;
	//�t�F���X�������l
	UINT64 _fenceVal = 0;
	//�t�F���X�̍쐬(����ŁAGPU�̖��߂����ׂĊ������������m���)
	result = _dev->CreateFence(_fenceVal,
		D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&_fence));

	//�E�B���h�E�̕\��
	ShowWindow(hwnd, SW_SHOW);

	// ���_���W�̒�`
	XMFLOAT3 vertices[] =
	{
		{-1.0f, -1.0f, 0.0f},
		{-1.0f, 1.0f, 0.0f},
		{1.0f, -1.0f, 0.0f}
	};
	//���_��GPU�ɓ`����ɂ́AGPU�Ƀo�b�t�@�[�̈���쐬���A�R�s�[����K�v������
	//ID3D12Resource���g�p���� -> CPU��new(malloc())�̂悤�Ȃ���
	//ID3D12Resource��CreateCommittedResource()�ō쐬�ł���

	//���_�q�[�v�̐ݒ�(�q�[�v�ݒ�)
	//typedef struct D3D12_HEAP_PROPERTIES {
	//	D3D12_HEAP_TYPE         Type;		//�q�[�v�̎��
	//	D3D12_CPU_PAGE_PROPERTY CPUPageProperty;			//�q�[�v��CPU�y�[�W�^�C�v
	//	D3D12_MEMORY_POOL       MemoryPoolPreference;		//�q�[�v�̃������v�[���^�C�v
	//	UINT                    CreationNodeMask;			//�}���`�A�_�v�^�[����̏ꍇ�A����́A���\�[�X���쐬����K�v������m�[�h(����͒P��A�_�v�^�[�Ȃ̂�0)
	//	UINT                    VisibleNodeMask;			//�}���`�A�_�v�^�[����̏ꍇ�A����̓��\�[�X���\�������m�[�h�̃Z�b�g(����͒P��A�_�v�^�[�Ȃ̂�0)
	//} D3D12_HEAP_PROPERTIES;
	D3D12_HEAP_PROPERTIES heapProp = {};
	//typedef enum D3D12_HEAP_TYPE {		//�q�[�v���
	//	D3D12_HEAP_TYPE_DEFAULT,		//CPU����A�N�Z�X�ł��Ȃ�(�}�b�v�ł��Ȃ�->ID3D12Resource::Map()�Ńq�[�v�ɃA�N�Z�X���悤�Ƃ���Ǝ��s����). �}�b�v�ł��Ȃ�����ɃA�N�Z�X�������AGPU�݂̂ň����q�[�v�Ƃ��ėD�G
	//	D3D12_HEAP_TYPE_UPLOAD,			//CPU����A�N�Z�X�ł���(�}�b�v�ł���->�쐬�����q�[�v��CPU����A�N�Z�X�ł��AID3D12Resource::Map()�Œ��g��������������)CPU�̃f�[�^��GPU�ɃA�b�v���[�h����̂Ɏg�p
	//�����AD3D12_HEAP_TYPE_DEFAULT�̃q�[�v�Ɣ�ׂ�ƒx�����߁A�������݁A�ǂݍ��݉񐔂𐧌������ق����ǂ�
	//	D3D12_HEAP_TYPE_READBACK,		//CPU����ǂݎ���(�Ăі߂���p�q�[�v. �o���h���͍L���Ȃ�(�܂�A�A�N�Z�X�͑����Ȃ�)�AGPU�ŉ��H�E�v�Z�����f�[�^��CPU�Ŋ��p���鎞�Ɏg�p. �ǂݎ���p�Ƃ�������)
	//	D3D12_HEAP_TYPE_CUSTOM			//�J�X�^���q�[�v
	// D3D12_HEAP_TYPE_CUSTOM�ȊO��I�������ꍇ�̓y�[�W�����N�ݒ�ƃ������[�v�[����UNKNOWN���w�肷��΂悢
	//};
	//�}�b�v�Ƃ�? -> �o�b�t�@�[��CPU�̃������A�N�Z�X�̂悤�Ɉ������߂̏����̂���
	heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
	// �q�[�v��CPU�y�[�W�^�C�v
	//typedef enum D3D12_CPU_PAGE_PROPERTY {
	//	D3D12_CPU_PAGE_PROPERTY_UNKNOWN,		//�q�[�vType��D3D12_HEAP_TYPE_CUSTOM�ȊO�̎��Ɏw��. CUSTOM�̎��ɂ�����w�肷�邱�Ƃ͂ł��Ȃ�
	//	D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE,		//CPU����̃A�N�Z�X�s��(GPU���̌v�Z�݂̂Ŏg�p����Ƃ�������. D3D12_HEAP_TYPE_DEFAULT�̐ݒ�ɋ߂�)
	//	D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE,		//CPU -> CPU�̃�����(�������̓L���b�V��������)�ɓ]��  -> �w���Ԃ��o������xGPU�ɓ]������(���C�g�o�b�N����)����
	//	D3D12_CPU_PAGE_PROPERTY_WRITE_BACK		// �f�[�^��������x�܂Ƃ߂đ������. �]�������͍l������Ȃ����ߒ���
	//};
	//typedef enum D3D12_MEMORY_POOL {
	//	D3D12_MEMORY_POOL_UNKNOWN,		//CUSTOM�ȊO�̎��͂���
	//	D3D12_MEMORY_POOL_L0,			//�V�X�e��������(CPU������. �f�B�X�N���[�gGPU�ł�CPU�o���h���͍L���AGPU�o���h���͋����Ȃ�). UMA�̎��͂��̑I�����̂�. 
	//	D3D12_MEMORY_POOL_L1			//�r�f�I������(GPU�o���h���͍L���Ȃ邪�ACPU������A�N�Z�X�ł��Ȃ��Ȃ�). �����^�O���t�B�b�N�X(UMA)�ł͎g�p�ł��Ȃ�
	//};
	heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProp.CreationNodeMask = 0;
	heapProp.VisibleNodeMask = 0;


	//���\�[�X�ݒ�\����
	//typedef struct D3D12_RESOURCE_DESC {
	//	D3D12_RESOURCE_DIMENSION Dimension;		//�g�p����Ă��郊�\�[�X�̃^�C�v�̐ݒ�
	//�@�o�b�t�@�[�Ɏg�p���邽�� D3D12_RESOURCE_DIMENSION_BUFFER ���w��
	//	UINT64                   Alignment;		//�z�u�̎w��
	//	UINT64                   Width;		//���ł��ׂĂ�d������sizeof(�S���_)�ɐݒ�. �摜�Ȃ畝���w��
	//	UINT                     Height;		//���őS�Ęd������1. �摜�Ȃ獂�����w��
	//	UINT16                   DepthOrArraySize;		//���\�[�X��3D�̏ꍇ�̓��\�[�X�̐[���A1D�܂���2D���\�[�X�̔z��̏ꍇ�͔z��T�C�Y. ����͉摜�ł͂Ȃ�����1�ł悢���ۂ�
	//	UINT16                   MipLevels;		//�~�b�v���x���̎w��
	//	DXGI_FORMAT              Format;		//���\�[�X�f�[�^�`���̎w��(RGBA�Ƃ�). �摜����Ȃ��̂�UNKNWON�w��
	//	DXGI_SAMPLE_DESC         SampleDesc;	//���\�[�X�̃}���`�T���v�����O�p�����[�^�ɂ��Ă̎w��(�A���`�G�C���A�V���O���g�p���鎞�͂����̒l��ς��Ă���)
		//typedef struct DXGI_SAMPLE_DESC {
		//	UINT Count;				//�}���`�T���v�����̐�(0�ɂ��Ă��܂��ƃf�[�^���Ȃ����ƂɂȂ�̂�1���w��)
		//	UINT Quality;			//���̃��x��(�f�t�H���g0���Ǝv��)
		//} DXGI_SAMPLE_DESC;
	//	D3D12_TEXTURE_LAYOUT     Layout;		//�e�N�X�`�����C�A�E�g�I�v�V�����̎w��. �e�N�X�`���ł͂Ȃ�����UNKNOWN���w��ł��Ȃ��B����͍ŏ�����Ō�܂Ń��������A�����Ă��邽��D3D12_TEXTURE_LAYOUT_ROW_MAJOR���w��
	// �e�N�Z���ł������w�肷��ꍇ�́AD3D12_TEXTURE_LAYOUT_ROW_MAJOR���w�肷��Ǝ��̍s�̃e�N�Z���̑O�Ƀ��������̍s�̘A�������e�N�Z����A�����Ĕz�u����炵��
	//	D3D12_RESOURCE_FLAGS     Flags;		//���\�[�X�𑀍삷�邽�߂̃I�v�V�����t���O(�������Ȃ��ꍇ��D3D12_RESOURCE_FLAG_NONE)
	//} D3D12_RESOURCE_DESC;
	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Alignment = 0;
	resourceDesc.Width = sizeof(vertices);
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;		//mip�����悤���Ȃ��̂ōŒ��1��
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	//ID3D12Resource�̍쐬
	ID3D12Resource* vertBuff = nullptr;
	result = _dev->CreateCommittedResource(
		&heapProp,		//�q�[�v�ݒ�\����
		D3D12_HEAP_FLAG_NONE,		//�q�[�v�̃I�v�V����(���ɉ������Ȃ��̂�D3D12_HEAP_FLAG_NONE). �q�[�v�����L������������A�o�b�t�@�[�܂߂����Ȃ��Ȃǂ���΂����Ŏw��
		&resourceDesc,		//���\�[�X�ݒ�\����
		D3D12_RESOURCE_STATE_GENERIC_READ,		//���\�[�X�̎g�p���@�Ɋւ��郊�\�[�X��Ԃ̐ݒ�(�����GPU����ǂݎ���p�ɂ������̂�D3D12_RESOURCE_STATE_GENERIC_READ)
		nullptr,		//�N���A�J���[�̃f�t�H���g�l�ݒ�(�����Texture�ł͂Ȃ����ߎg�p���Ȃ��̂�nullptr)
		IID_PPV_ARGS(&vertBuff)
	);

	//�����ɏI�����Ȃ��悤�Q�[�����[�v�̍쐬
	MSG msg = {};

	while (true)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		//�A�v���P�[�V�������I��炷�Ƃ���message��WM_QUIT�ɂȂ�
		if (msg.message == WM_QUIT)
		{
			break;
		}

		/////////////////////////////
		// �����艺��DirectX12�̏���
		/////////////////////////////

		/////////////////////////////
		// �X���b�v�`�F�[���̓���
		/////////////////////////////

		/////////////////////////////
		// 1. �R�}���h�A���P�[�^�[�̃��Z�b�g
		/////////////////////////////

		//result = _commandAllocator->Reset();
		//������Reset()����R�[�h���L��ƁA�u�A���P�[�^�[�̓R�}���h���X�g���L�^����Ă��邽�߃��Z�b�g���邱�Ƃ��o���Ȃ��v�ƕ\������Ă��܂��̂ŏ����Ă���

		//���̂��Ƃ����ɁA���߃I�u�W�F�N�g�𒙂߂Ă���

		/////////////////////////////
		// 2.�����_�[�^�[�Q�b�g�̐ݒ�(�o�b�N�o�t�@�[��ݒ肷��)
		/////////////////////////////

		//���݂̃o�b�N�o�b�t�@�[��index�擾(���̂��߂ɂ�IDXGISwapChain3�ȏ��swapChain��������K�v������)
		//�����IDXGISwapChain4�ŃX���b�v�`�F�[����ۑ����Ă���
		auto backBufferIndex = _swapchain->GetCurrentBackBufferIndex();
		//���݂̃o�b�N�o�b�t�@�[��index��ۑ������̂ŁA���̒l�͎��̃t���[���ŕ`�悳��邱�ƂɂȂ�

		/////////////////////////////
		// �o���A�̐ݒ�
		/////////////////////////////

		D3D12_RESOURCE_BARRIER BarrierDesc = {};
		BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;		//�J�ڃo���A
		BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;			//�����o���A���s���킯�ł͂Ȃ��̂�NONE
		BarrierDesc.Transition.pResource = _backBuffers[backBufferIndex];		//�o�b�N�o�b�t�@�[�̃��\�[�X�ɂ��Đ��������邱�Ƃ�GPU�ɓ`����
		BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;				//1�̃o�b�N�o�b�t�@�[�̂݃o���A�̎w�������̂�0

		// PRESENT��Ԃ��I���܂Ń����_�[�^�[�Q�b�g���ҋ@�B�o���A���s��̓����_�[�^�[�Q�b�g�Ƃ��Ďg�p���ƂȂ�
		//PRESENT = D3D12_RESOURCE_STATE_COMMON�Ɠ��`
		BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;			//���O��PRESENT���
		BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;		//�����烌���_�[�^�[�Q�b�g��Ԃɂ����

		//�o�b�N�o�b�t�@���u�����_�[�^�[�Q�b�g�Ƃ��Ďg�p����v���Ƃ�GPU�ɒm�点��
		_commandList->ResourceBarrier(
			1,		//�o�b�N�o�b�t�@�[1�����Ȃ̂�1���w��
			&BarrierDesc
		);

		// �����_�[�^�[�Q�b�g�̎w��

		//index0�̃o�b�N�o�b�t�@�[�̃|�C���^�[���擾
		D3D12_CPU_DESCRIPTOR_HANDLE renderTargetViewHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
		//�����_�[�^�[�Q�b�g�r���[�Ƃ��Ďw�肵�����o�b�N�o�b�t�@�[�̃|�C���^��ݒ�
		renderTargetViewHandle.ptr += backBufferIndex * _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		//�R�}���h���X�g���Ă�ł���֐�-> �R�}���h���X�g�𒙂߂Ă���
		_commandList->OMSetRenderTargets(1,
			&renderTargetViewHandle,
			false,		//2�̃����_�[�^�[�Q�b�g�r���[�͔z��̂悤�ɕ���ł��邽��true�ɂ���
			nullptr);


		/////////////////////////////
		// 3.�����_�[�^�[�Q�b�g���w��F�ŃN���A
		/////////////////////////////

		//�����_�[�^�[�Q�b�g�����̐F�ŃN���A
		float clearColor[] = { 1.0f, 1.0f, 0.0f, 1.0f };		//���F
		/*
		�����_�[�^�[�Q�b�g�̂��ׂĂ̗v�f��1�̒l�ɐݒ�
		void ClearRenderTargetView(
		  D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView,		//�N���A���郌���_�[�^�[�Q�b�g�n���h��
		  const FLOAT [4]             ColorRGBA,			//�����_�[�^�[�Q�b�g��h��Ԃ��FRGBA�z��
		  UINT                        NumRects,				//pReacts���w�肷��z����̒����`�̐�
		  const D3D12_RECT            *pRects				//�N���A�����`(D3D12_RECT �\����)�̔z��. nullptr���w�肵���ꍇ(�ݒ肵���o�b�N�o�b�t�@��)���\�[�X�r���[�S��
		);
		*/
		//�R�}���h���X�g�ɒ��߂Ă���
		_commandList->ClearRenderTargetView(renderTargetViewHandle, clearColor, 0, nullptr);

		BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;			//���O��PRESENT���
		BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;		//�����烌���_�[�^�[�Q�b�g��Ԃɂ����


		//�o�b�N�o�b�t�@���u�����_�[�^�[�Q�b�g�Ƃ��Ďg�p����v���Ƃ�GPU�ɒm�点��
		//void ResourceBarrier(
		//	UINT                         NumBarriers,		//�ݒ肷��o���A����(D3D12_RESOURCE_BARRIER)�̐�
		//	const D3D12_RESOURCE_BARRIER* pBarriers		//�ݒ肷��o���A�����\����(�z��)�̃A�h���X => �z��Ƃ��ĕ����̃o���A���܂Ƃ߂Đݒ�ł���
		//);
		_commandList->ResourceBarrier(
			1,		//�o�b�N�o�b�t�@�[1�����Ȃ̂�1���w��
			&BarrierDesc
		);

		/////////////////////////////
		// 4. ���߂Ă��������߂̎��s
		/////////////////////////////

		// ����; ���s�O�ɃR�}���h���X�g�̓N���[�Y���Ȃ���΂Ȃ�Ȃ�

		//�R�}���h�̃N���[�Y
		_commandList->Close();

		//�R�}���h�̎��s
		ID3D12CommandList* commandList[] = { _commandList };		//ID3D12GraphicsCommandList��ID3D12CommandList�C���^�[�t�F�[�X���p�����Ă���
		//�R�}���h�L���[�ŃR�}���h�̎��s(�R�}���h�z��̑��M)
		_commandQueue->ExecuteCommandLists(
			1,
			commandList
		);

		//�t�F���X�͓����ɃJ�E���^�������Ă���AGetCompletedValue()�Ŏ擾�ł���
		//Signal()�Őݒ肪�ł���

		//�R�}���h���s��AGPU�����߂����ׂĊ�������܂ő҂�
		//ID3D12CommandQueue::Signal()���ĂԂƁA
		//�ȑO�ɃT�u�~�b�g(�܂茻�݂̃��[�v���ɓ����ꂽ)���ꂽ�R�}���h��GPU��ł̎��s���������Ă���A���邢�͊������I������Ƃ��A�����I�ɑ�1�����Ŏw�肵��Fence�̒l���2�����̒l�ɍX�V���܂�
		//�h�L�������g�ł�fencen�̂��̒l��" fence value "�ƌĂсA���� �O�̒l���1�傫���l���w�肷��
		_commandQueue->Signal(_fence, ++_fenceVal);

		//�r�W�[���[�v���g�p���Ȃ����@
		//Window�̃C�x���g��WaitForSingleObject()��p������@������

		//���ł�GPU�̏������I���Ă�����C�x���g�̐ݒ�͂��Ȃ��ł悢�̂ŁA�`�F�b�N����
		if (_fence->GetCompletedValue() != _fenceVal)
		{
			//�C�x���g�n���h���̎擾
			auto event = CreateEvent(
				nullptr,		//�Z�L�����e�B�L�q�q (NULL���w�肷��ƃf�t�H���g�l������)
				false,			// TRUE�Ȃ�蓮���Z�b�g�I�u�W�F�N�g���AFALSE�Ȃ玩�����Z�b�g�I�u�W�F�N�g���쐬�����B
				//�蓮���Z�b�g�I�u�W�F�N�g��ResetEvent�֐���CALL���ăC�x���g�I�u�W�F�N�g���V�O�i����Ԃɂ�����̂ŁA�������Z�b�g�I�u�W�F�N�g��WaitForSingleObject�֐���CALL�����㎩���I�ɔ�V�O�i����ԂɂȂ�B
				//�V�O�i����� .. �C�x���g���g�p�\���, ��V�O�i�� .. �C�x���g���g�p�s�\���
				false,			//�V�O�i���̏�����Ԃ��L�q. TRUE�Ȃ�V�O�i�����, false�Ȃ��V�O�i����� 
				nullptr			//�C�x���g�I�u�W�F�N�g�̖��O
			);

			_fence->SetEventOnCompletion(
				_fenceVal,
				event
			);

			//�w�肳�ꂽ�C�x���g�I�u�W�F�N�g���V�O�i����ԂɂȂ邩�A�w�肳�ꂽ���Ԃ��o�߂���܂ŃX���b�h��ҋ@
			WaitForSingleObject(event,//�����I�u�W�F�N�g�̃n���h��
				INFINITE		//�C�x���g�I�u�W�F�N�g�̏�Ԃ��V�O�i����ԂɂȂ�܂ł̑ҋ@���鎞��.�w�肵�����ԓ��ɃV�O�i����ԂɂȂ�Ȃ���΁AWaitForSingleObject�֐��͌Ăяo�����ɐ����Ԃ�
			);

			//�C�x���g�n���h�������
			CloseHandle(event);
		}

		//�A���P�[�^�[���̃L���[���N���A
		_commandAllocator->Reset();
		//�R�}���h���X�g�ɂ܂����߂���悤����(�N���[�Y�����Ƃ�)
		_commandList->Reset(_commandAllocator, nullptr);


		/////////////////////////////
		// 5. ��ʂ̃X���b�v
		/////////////////////////////
		//��ʂ̃X���b�v(�t���b�v)���s��
		_swapchain->Present(1, 0);
	}

	//���̃N���X�͎g��Ȃ��̂œo�^����
	UnregisterClass(w.lpszClassName, w.hInstance);

	return 0;
}