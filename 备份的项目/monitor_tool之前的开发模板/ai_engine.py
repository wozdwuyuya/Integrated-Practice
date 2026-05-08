import pandas as pd
import numpy as np
from sklearn.ensemble import RandomForestClassifier
from sklearn.model_selection import train_test_split
from sklearn.metrics import accuracy_score
import joblib
import os

# --- 1. 启发式信号质量评估 (SQI Module) ---
def classify_signal_heuristic(raw_val, std_val):
    """
    根据阈值进行实时状态监测 (Heuristic)
    硬阈值优先：如果 Raw_Value < 120，直接判定为脱落
    """
    if raw_val < 120:
        return "DISCONNECTED" # 脱落 (硬阈值拦截)
    if std_val > 35:
        return "INTERFERENCE" # 干扰
    return "HEALTHY"          # 健康

# --- 2. 机器学习模型训练与导出 ---
def train_heart_model(healthy_csv, interference_csv, model_path='heart_model.pkl'):
    """
    使用随机森林算法进行二分类训练：
    Label 0: 健康 (Healthy)
    Label 1: 干扰/脱落 (Anomaly)
    优化：剔除 Raw_Value < 120 的极端值，让模型专注于区分“稳定”和“干扰”
    """
    print(f"正在读取并清洗数据文件...")
    df_healthy = pd.read_csv(healthy_csv)
    df_interference = pd.read_csv(interference_csv)

    # 标注数据：健康为 0，干扰为 1
    df_healthy['Label'] = 0
    df_interference['Label'] = 1

    # 合并数据集
    df = pd.concat([df_healthy, df_interference], ignore_index=True)
    
    # 核心优化：剔除硬阈值范围的数据，减少噪声干扰训练
    df = df[df['Raw_Value'] >= 120]
    
    if len(df) < 50:
        print("警告：清洗后数据量不足，请采集更多数据。")
        return 0

    # 特征选择：使用 Raw_Value 和 STD_10
    X = df[['Raw_Value', 'STD_10']]
    y = df['Label']

    # 划分训练集和测试集
    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)

    # 初始化并训练随机森林
    model = RandomForestClassifier(n_estimators=100, random_state=42)
    model.fit(X_train, y_train)

    # 评估准确率
    y_pred = model.predict(X_test)
    acc = accuracy_score(y_test, y_pred)
    
    print(f"==========================================")
    print(f"  AI 模型训练完成！(已剔除硬阈值数据)")
    print(f"  准确率 (Accuracy): {acc:.2%}")
    print(f"==========================================")

    # 导出模型
    joblib.dump(model, model_path)
    print(f"模型已保存至: {os.path.abspath(model_path)}")
    return acc

# --- 3. 实时推理类 (用于在线监测) ---
class HeartAI:
    def __init__(self, model_path='heart_model.pkl'):
        self.model = None
        if os.path.exists(model_path):
            try:
                self.model = joblib.load(model_path)
                print(f"AI 加载成功: {model_path}")
            except:
                print("AI 模型加载失败，将回退到启发式逻辑。")
        else:
            print("未发现模型文件，将使用启发式逻辑。")

    def predict(self, raw_val, std_val):
        """
        实时预测当前状态：
        返回: (status_text, label)
        """
        # 1. 启发式判断作为第一道防线 (硬阈值拦截)
        status = classify_signal_heuristic(raw_val, std_val)
        
        # 2. 最强安静判定 (Strong Quiet Detection)
        # 如果信号非常稳定 (STD < 15) 且 数值正常 (Raw > 150)，强制 HEALTHY，不调 AI
        if std_val < 15.0 and raw_val > 150:
            return "HEALTHY", 0

        # 3. 如果启发式已经判定为 DISCONNECTED，直接返回，不调 AI
        if status == "DISCONNECTED":
            return status, 1
            
        # 4. 如果模型可用，且不在上述硬拦截范围内，再用 AI 细分
        if self.model:
            # 使用 pd.DataFrame 并指定列名以消除 sklearn 警告
            X = pd.DataFrame([[raw_val, std_val]], columns=['Raw_Value', 'STD_10'])
            label = self.model.predict(X)[0]
            # 如果 AI 判定为 1 (异常)，即使启发式认为健康也标记为干扰
            if label == 1:
                status = "INTERFERENCE"
            else:
                status = "HEALTHY" # 如果 AI 说健康
        else:
            label = 1 if status != "HEALTHY" else 0
            
        return status, label

# 如果作为主程序运行，则执行训练
if __name__ == "__main__":
    # 查找本地生成的 CSV 文件进行训练
    # 假设文件名与用户描述一致
    h_file = 'ai_training_data_20260318_221012.csv'
    i_file = 'ai_training_data_20260318_221940.csv'
    
    if os.path.exists(h_file) and os.path.exists(i_file):
        train_heart_model(h_file, i_file)
    else:
        print("未找到训练所需的 CSV 文件，请确保已运行 data_factory.py 采集数据。")
