//エントリポイントの指定は、シェーダーファイルで「プロパティ」->「エントリポイント名」を変更する(関数名に)
float4 BasicVS( float4 pos : POSITION ) : SV_POSITION		//BasicVS()は1頂点ごとに呼ばれる(頂点がposという引数で来る)
// POSITION, SV_POSITIONはセマンティックス(データをどういう用途で使用するのか教えるもの). 
//POSITIONは座標として使用する際に指定する
//SV_POSITONは関数の返り値のfloat4にかかっており、(頂点シェーダーのような)プログラマブルなステージから、(ラスタライズのような)非プログラマブルなステージに情報を投げるものには接頭辞にSV_を付ける必要がある
{
	return pos;
}