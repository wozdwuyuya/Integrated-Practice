package com.healthmonitor.app;

import android.content.SharedPreferences;
import android.graphics.Color;
import android.graphics.Typeface;
import android.os.Bundle;
import android.text.method.ScrollingMovementMethod;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

public class AiConsultActivity extends AppCompatActivity {

    private LinearLayout chatContainer;
    private ScrollView scrollView;
    private EditText etInput;
    private Button btnSend;
    private View loadingBar;
    private TextView tvCurrentData;
    private HealthRuleEngine ruleEngine;
    private AiService aiService;
    private HealthData currentData;
    private boolean isLoading = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_ai_consult);

        ruleEngine = new HealthRuleEngine(this);

        SharedPreferences prefs = getSharedPreferences("health_monitor", MODE_PRIVATE);
        String host = prefs.getString("host", "192.168.4.1");
        aiService = new AiService(host);

        initViews();
        setupButtons();
        loadCurrentData();
        showWelcomeMessage();
    }

    private void initViews() {
        chatContainer = findViewById(R.id.chatContainer);
        scrollView = findViewById(R.id.scrollView);
        etInput = findViewById(R.id.etInput);
        btnSend = findViewById(R.id.btnSend);
        loadingBar = findViewById(R.id.loadingBar);
        tvCurrentData = findViewById(R.id.tvCurrentData);
    }

    private void setupButtons() {
        findViewById(R.id.btnBack).setOnClickListener(v -> finish());
        findViewById(R.id.btnClear).setOnClickListener(v -> {
            aiService.clearHistory();
            chatContainer.removeAllViews();
            addDataCard();
            addQuickActions();
            showWelcomeMessage();
        });

        btnSend.setOnClickListener(v -> sendMessage());

        findViewById(R.id.btnQuick1).setOnClickListener(v -> sendQuickMessage("分析我当前的健康状况"));
        findViewById(R.id.btnQuick2).setOnClickListener(v -> sendQuickMessage("我感觉头晕，应该怎么办？"));
        findViewById(R.id.btnQuick3).setOnClickListener(v -> sendQuickMessage("给我一些改善睡眠的建议"));
        findViewById(R.id.btnQuick4).setOnClickListener(v -> sendQuickMessage("如何预防心血管疾病？"));
    }

    private void loadCurrentData() {
        currentData = getIntent().getParcelableExtra("health_data");
        if (currentData == null) {
            currentData = new HealthData();
        }
        updateDataDisplay();
    }

    private void updateDataDisplay() {
        String dataText = String.format(
                "心率：%d 次/分  |  血氧：%.1f%%  |  体温：%.1f°C\n跌倒置信度：%d%%  |  %s",
                currentData.getHeartRate(),
                currentData.getSpo2(),
                currentData.getTemperature(),
                currentData.getFallConfidence(),
                currentData.getStatus()
        );
        tvCurrentData.setText(dataText);
    }

    private void showWelcomeMessage() {
        addAiMessage("你好！我是你的 AI 健康助手 🤖\n\n" +
                "我可以根据你当前的健康监测数据为你提供专业的健康分析和建议。\n\n" +
                "你可以：\n" +
                "• 点击上方的快捷问诊按钮\n" +
                "• 或在下方输入框描述你的症状和问题\n\n" +
                "⚠️ 提醒：我的建议仅供参考，不替代专业医疗诊断。");
    }

    private void sendMessage() {
        String input = etInput.getText().toString().trim();
        if (input.isEmpty() || isLoading) return;
        etInput.setText("");
        sendQuickMessage(input);
    }

    private void sendQuickMessage(String message) {
        if (isLoading) return;

        addUserMessage(message);
        String ragContext = ruleEngine.buildRagContext(currentData, message);
        aiService.ask(ragContext, message, new AiService.AiCallback() {
            @Override
            public void onSuccess(String response) {
                addAiMessage(response);
            }

            @Override
            public void onError(String error) {
                addSystemMessage("⚠️ " + error + "\n\n请检查服务器连接是否正常，或稍后重试。");
            }

            @Override
            public void onLoading(boolean loading) {
                isLoading = loading;
                loadingBar.setVisibility(loading ? View.VISIBLE : View.GONE);
                btnSend.setEnabled(!loading);
                btnSend.setText(loading ? "等待中..." : "发送");
            }
        });
    }

    private void addUserMessage(String text) {
        LinearLayout wrapper = new LinearLayout(this);
        wrapper.setLayoutParams(new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        wrapper.setGravity(Gravity.END);
        wrapper.setPadding(dpToPx(60), dpToPx(8), 0, dpToPx(8));

        TextView tv = new TextView(this);
        tv.setText(text);
        tv.setTextColor(Color.WHITE);
        tv.setTextSize(14);
        tv.setPadding(dpToPx(14), dpToPx(10), dpToPx(14), dpToPx(10));

        android.graphics.drawable.GradientDrawable bg = new android.graphics.drawable.GradientDrawable();
        bg.setColor(getColor(R.color.accent_blue));
        bg.setCornerRadius(dpToPx(14));
        tv.setBackground(bg);

        wrapper.addView(tv);
        chatContainer.addView(wrapper);
        scrollToBottom();
    }

    private void addAiMessage(String text) {
        LinearLayout wrapper = new LinearLayout(this);
        wrapper.setLayoutParams(new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        wrapper.setGravity(Gravity.START);
        wrapper.setPadding(0, dpToPx(8), dpToPx(60), dpToPx(8));

        LinearLayout bubbleLayout = new LinearLayout(this);
        bubbleLayout.setOrientation(LinearLayout.VERTICAL);
        bubbleLayout.setPadding(dpToPx(14), dpToPx(10), dpToPx(14), dpToPx(10));

        android.graphics.drawable.GradientDrawable bg = new android.graphics.drawable.GradientDrawable();
        bg.setColor(getColor(R.color.bg_card));
        bg.setCornerRadius(dpToPx(14));
        bg.setStroke(dpToPx(1), getColor(R.color.border_color));
        bubbleLayout.setBackground(bg);

        TextView label = new TextView(this);
        label.setText("🤖 AI 助手");
        label.setTextColor(getColor(R.color.accent_blue));
        label.setTextSize(11);
        label.setTypeface(null, Typeface.BOLD);
        bubbleLayout.addView(label);

        TextView content = new TextView(this);
        content.setText(text);
        content.setTextColor(getColor(R.color.text_primary));
        content.setTextSize(14);
        content.setLineSpacing(0, 1.4f);
        content.setMovementMethod(new ScrollingMovementMethod());
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.topMargin = dpToPx(6);
        content.setLayoutParams(lp);
        bubbleLayout.addView(content);

        wrapper.addView(bubbleLayout);
        chatContainer.addView(wrapper);
        scrollToBottom();
    }

    private void addSystemMessage(String text) {
        LinearLayout wrapper = new LinearLayout(this);
        wrapper.setLayoutParams(new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        wrapper.setGravity(Gravity.CENTER);
        wrapper.setPadding(dpToPx(30), dpToPx(8), dpToPx(30), dpToPx(8));

        TextView tv = new TextView(this);
        tv.setText(text);
        tv.setTextColor(getColor(R.color.text_secondary));
        tv.setTextSize(12);
        tv.setGravity(Gravity.CENTER);
        tv.setLineSpacing(0, 1.4f);

        wrapper.addView(tv);
        chatContainer.addView(wrapper);
        scrollToBottom();
    }

    private void addDataCard() {
        View dataCard = getLayoutInflater().inflate(R.layout.activity_ai_consult, chatContainer, false);
    }

    private void addQuickActions() {
    }

    private void scrollToBottom() {
        scrollView.post(() -> scrollView.fullScroll(View.FOCUS_DOWN));
    }

    private int dpToPx(int dp) {
        return (int) (dp * getResources().getDisplayMetrics().density);
    }
}
