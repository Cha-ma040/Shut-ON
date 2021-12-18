from enum import Enum
from logging import error

import librosa
import numpy as np


class Feature_Types(Enum):
    SPECTRUM = 1
    SPECTRUM_CENTROID = 2
    MFCC = 3

    def comparator():
        # サンプリング周波数の指定
        SAMPLING_FREQ = 44100

        # 使用する特徴量の種類
        feature_type = Feature_Types.SPECTRUM_CENTROID

        # 各wavファイルの振幅データ列とサンプリング周波数の取得
        high, h_sr = librosa.load("high.wav", SAMPLING_FREQ)
        low, l_sr = librosa.load("low.wav", SAMPLING_FREQ)
        val, v_sr = librosa.load("val.wav", SAMPLING_FREQ)

        # 特徴抽出
        if feature_type == Feature_Types.SPECTRUM:
            high_feature = np.abs(librosa.stft(high, h_sr))
            low_feature = np.abs(librosa.stft(low, l_sr))
            val_feature = np.abs(librosa.stft(val, l_sr))
        elif feature_type == Feature_Types.SPECTRUM_CENTROID:
            high_feature = librosa.feature.spectral_centroid(high, h_sr)
            low_feature = librosa.feature.spectral_centroid(low, l_sr)
            val_feature = librosa.feature.spectral_centroid(val, l_sr)
        else:
            raise Exception('指定された特徴量の種類がおかしいです。')

        # 類似度計算
        ac_high, _ = librosa.sequence.dtw(val_feature, high_feature)
        ac_low, _ = librosa.sequence.dtw(val_feature, low_feature)
        eval_high = 1 - (ac_high[-1][-1] / np.array(ac_high).max())
        eval_low = 1 - (ac_low[-1][-1] / np.array(ac_low).max())

        if eval_high >= eval_low:
            return "high"
        else:
            return "row"

if __name__ == "__main__":
    NotImplementedError
