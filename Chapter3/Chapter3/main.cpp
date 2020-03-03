#include <Windows.h>
#ifdef  _DEBUG
#include <iostream>
#endif //  _DEBUG

using namespace std;

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

#if _DEBUG		//�f�o�b�O���[�h(�ŏ��̕��̓R�}���h���C���o�͂ŏ��\�����邽��)
int main()
{
#else // _DEBUG
// Windows�v���O�����̎�v�ȃC���N���[�h�t�@�C����WINDOWS.H
// Windows �v���O������main()�֐��͖���. �G���g���[�|�C���g��WinMain()
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)		//��f�o�b�O���[�h
{
#endif
	DebugOutputFormatString("Show windows test");
	getchar();		//�\�����������������Ȃ��悤�ɂ���
	return 0;
}