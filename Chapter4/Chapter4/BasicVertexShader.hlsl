//�G���g���|�C���g�̎w��́A�V�F�[�_�[�t�@�C���Łu�v���p�e�B�v->�u�G���g���|�C���g���v��ύX����(�֐�����)
float4 BasicVS( float4 pos : POSITION ) : SV_POSITION		//BasicVS()��1���_���ƂɌĂ΂��(���_��pos�Ƃ��������ŗ���)
// POSITION, SV_POSITION�̓Z�}���e�B�b�N�X(�f�[�^���ǂ������p�r�Ŏg�p����̂����������). 
//POSITION�͍��W�Ƃ��Ďg�p����ۂɎw�肷��
//SV_POSITON�͊֐��̕Ԃ�l��float4�ɂ������Ă���A(���_�V�F�[�_�[�̂悤��)�v���O���}�u���ȃX�e�[�W����A(���X�^���C�Y�̂悤��)��v���O���}�u���ȃX�e�[�W�ɏ��𓊂�����̂ɂ͐ړ�����SV_��t����K�v������
{
	return pos;
}