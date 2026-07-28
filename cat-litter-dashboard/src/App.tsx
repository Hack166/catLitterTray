import { useState, useEffect } from 'react'
import './App.css'

function App() {
  // === L17: 滚筒霍尔 (H1-H4) ===
  const [halls, setHalls] = useState({ h1: false, h2: false, h3: false, h4: true });
  
  // === L16: 电机状态 ===
  const [motorStatus, setMotorStatus] = useState<'IDLE' | 'FORWARD' | 'REVERSE'>('IDLE');
  
  // === L2: 称重 ===
  const [weight, setWeight] = useState<number>(0.0);
  
  // === TOF: 激光测距 ===
  const [tofDistance, setTofDistance] = useState<number>(850);
  
  // === L3: 温湿度 ===
  const [temp, setTemp] = useState<number>(26.5);
  const [humidity, setHumidity] = useState<number>(48);

  // === 结构安全 (L7 顶盖, L4 抽屉, L14 热释电) ===
  const [topCoverOpen, setTopCoverOpen] = useState(false);
  const [drawerOpen, setDrawerOpen] = useState(false);
  const [pirMotion, setPirMotion] = useState(false);

  // === 光电对管 (L8/19 下, L9/18 上, L10/11 堆尖) ===
  const [irTopBlocked, setIrTopBlocked] = useState(false);
  const [irBottomBlocked, setIrBottomBlocked] = useState(false);
  const [irWasteFull, setIrWasteFull] = useState(false);

  const getMachineStatus = () => {
    return motorStatus === 'IDLE' ? '空闲' : '运行中';
  };
  
  // 模拟数据生成
  useEffect(() => {
    const interval = setInterval(() => {
      const actions: ('IDLE' | 'FORWARD' | 'REVERSE')[] = ['IDLE', 'FORWARD', 'IDLE', 'REVERSE'];
      const nextMotor = actions[Math.floor(Math.random() * actions.length)];
      setMotorStatus(nextMotor);
      
      if (nextMotor !== 'IDLE') {
        const rand = Math.floor(Math.random() * 5);
        setHalls({ h1: rand === 1, h2: rand === 2, h3: rand === 3, h4: rand === 4 });
      }
      
      setWeight(+(Math.random() * 6).toFixed(2)); 
      setTofDistance(Math.floor(Math.random() * 500) + 100); 
      setTemp(+(25 + Math.random() * 3).toFixed(1));
      setHumidity(Math.floor(40 + Math.random() * 20));

      setTopCoverOpen(Math.random() > 0.95);
      setDrawerOpen(Math.random() > 0.9);
      setPirMotion(Math.random() > 0.7);

      setIrTopBlocked(Math.random() > 0.85);
      setIrBottomBlocked(Math.random() > 0.85);
      setIrWasteFull(Math.random() > 0.95);
    }, 1500);
    return () => clearInterval(interval);
  }, []);

  return (
    <div className="gh-layout">
      {/* 顶部通栏 */}
      <header className="gh-header">
        <div className="gh-header-logo">智能猫砂盆控制中枢</div>
        <div className="gh-header-badge">设备在线</div>
      </header>

      <main className="gh-main">

        {/* --- 顶部区域: 监控画面与核心驱动 --- */}
        <div className="gh-grid-top">
          
          {/* L12: 内部监控画面 */}
          <div className="gh-box camera-box">
            <div className="gh-box-header">
              <span className="gh-box-title">L12: 内部监控画面 (ESP32-CAM)</span>
              <span className="gh-badge active"><div className="dot"></div> 直播中</span>
            </div>
            <div className="gh-box-body camera-body">
              <div className="camera-feed-placeholder">
                <span className="camera-text">无视频流信号 / 摄像头未就绪</span>
                <div className="camera-overlay-stats">
                  <span>帧率: 30 fps</span>
                  <span>分辨率: 800x600</span>
                  <span>{new Date().toLocaleTimeString()}</span>
                </div>
              </div>
            </div>
          </div>

          {/* 行动控制与主轴状态 */}
          <div className="gh-box core-box" style={{ display: 'flex', flexDirection: 'column' }}>
            <div className="gh-box-header">
              <span className="gh-box-title">驱动与控制中心</span>
            </div>
            <div className="gh-box-body" style={{ display: 'flex', flexDirection: 'column', gap: '24px', flex: 1 }}>
              
              {/* 控制按钮区 */}
              <div>
                <div style={{ fontSize: '12px', fontWeight: '600', color: '#8b949e', marginBottom: '12px' }}>快速操作指令</div>
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
                  <button className="gh-btn gh-btn-primary">开始清理</button>
                  <button className="gh-btn">一键倒沙</button>
                  <button className="gh-btn">复位归零</button>
                  <button className="gh-btn gh-btn-danger">紧急停止</button>
                </div>
              </div>

              <hr style={{ border: 'none', borderTop: '1px solid #30363d', margin: 0 }} />

              {/* 机器运行状态 */}
              <div className="gh-motor-area">
                <div className={`gh-motor-spin ${motorStatus === 'FORWARD' ? 'fwd' : motorStatus === 'REVERSE' ? 'rev' : ''}`}>
                  <div className="gh-motor-text">
                    {getMachineStatus()}
                  </div>
                </div>
                <div style={{ fontSize: '14px', color: '#8b949e' }}>
                  <div className="value" style={{ color: motorStatus === 'IDLE' ? '#8b949e' : '#3fb950', fontSize: '24px', fontWeight: 'bold' }}>
                    {getMachineStatus()}
                  </div>
                </div>
              </div>

              {/* L17 霍尔传感器阵列 */}
              <div>
                <div style={{ fontSize: '12px', fontWeight: '600', color: '#8b949e', marginBottom: '12px' }}>L17 滚筒绝对定位编码器</div>
                <div className="gh-status-row">
                  <div className={`gh-badge ${halls.h1 ? 'active' : ''}`}>
                    <div className="dot"></div> H1
                  </div>
                  <div className={`gh-badge ${halls.h2 ? 'active' : ''}`}>
                    <div className="dot"></div> H2
                  </div>
                  <div className={`gh-badge ${halls.h3 ? 'active' : ''}`}>
                    <div className="dot"></div> H3
                  </div>
                  <div className={`gh-badge ${halls.h4 ? 'active' : ''}`}>
                    <div className="dot"></div> H4 (原点)
                  </div>
                </div>
              </div>
            </div>
          </div>
        </div>

        {/* --- 中部区域: 环境与安全传感器 --- */}
        <div className="gh-grid-3">
          
          {/* 环境数据 */}
          <div className="gh-box">
            <div className="gh-box-header">
              <span className="gh-box-title">环境与健康指标</span>
            </div>
            <div className="gh-box-body">
              <ul className="gh-list">
                <li>
                  <span className="label">L2 称重</span>
                  <span className="value highlight-blue">{weight > 0 ? `${weight} kg` : '0.00 kg'}</span>
                </li>
                <li>
                  <span className="label">TOF 激光测距 (猫咪靠近)</span>
                  <span className="value">{tofDistance} mm</span>
                </li>
                <li>
                  <span className="label">L3 舱内温度</span>
                  <span className="value">{temp} °C</span>
                </li>
                <li>
                  <span className="label">L3 舱内湿度</span>
                  <span className="value">{humidity} %</span>
                </li>
              </ul>
            </div>
          </div>

          {/* 结构保护 */}
          <div className="gh-box">
            <div className="gh-box-header">
              <span className="gh-box-title">安全与物理保护</span>
            </div>
            <div className="gh-box-body">
              <ul className="gh-list">
                <li>
                  <span className="label">L7 顶盖状态</span>
                  <span className={`gh-badge ${topCoverOpen ? 'danger' : 'active'}`}>
                    <div className="dot"></div> {topCoverOpen ? '被掀开 (警告)' : '已闭合'}
                  </span>
                </li>
                <li>
                  <span className="label">L4 集便抽屉</span>
                  <span className={`gh-badge ${drawerOpen ? 'danger' : 'active'}`}>
                    <div className="dot"></div> {drawerOpen ? '已拉出 (警告)' : '已推入'}
                  </span>
                </li>
                <li>
                  <span className="label">L14 热释电 (活体移动)</span>
                  <span className={`gh-badge ${pirMotion ? 'warn' : ''}`}>
                    <div className="dot"></div> {pirMotion ? '检测到移动' : '未检测到目标'}
                  </span>
                </li>
              </ul>
            </div>
          </div>

          {/* 光电对管 */}
          <div className="gh-box">
            <div className="gh-box-header">
              <span className="gh-box-title">光电对射检测</span>
            </div>
            <div className="gh-box-body">
              <ul className="gh-list">
                <li>
                  <span className="label">L9/L18 顶部防夹光栅</span>
                  <span className={`gh-badge ${irTopBlocked ? 'warn' : 'active'}`}>
                    <div className="dot"></div> {irTopBlocked ? '有异物遮挡' : '通畅'}
                  </span>
                </li>
                <li>
                  <span className="label">L8/L19 底部防夹光栅</span>
                  <span className={`gh-badge ${irBottomBlocked ? 'warn' : 'active'}`}>
                    <div className="dot"></div> {irBottomBlocked ? '有异物遮挡' : '通畅'}
                  </span>
                </li>
                <li>
                  <span className="label">L10/L11 集便盆满载堆尖</span>
                  <span className={`gh-badge ${irWasteFull ? 'danger' : 'active'}`}>
                    <div className="dot"></div> {irWasteFull ? '容量已满！' : '未满'}
                  </span>
                </li>
              </ul>
            </div>
          </div>
        </div>

      </main>
    </div>
  )
}

export default App
