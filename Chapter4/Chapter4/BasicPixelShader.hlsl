//ピクセルシェーダー
//TARGET: レンダーターゲットに書き込む情報であることを伝えるセマンティックス
//レンダーターゲットへの書き込み(非プログラマブルなステージ)に情報を流すため、SV_という接頭辞がついている
float4 BasicPS(float4 pos : POSITION) : SV_TARGET
{
	return float4(1.0f, 1.0f, 1.0f, 1.0f);
}