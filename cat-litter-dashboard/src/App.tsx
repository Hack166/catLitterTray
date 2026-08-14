import { FormEvent, useEffect, useRef, useState } from 'react'
import './App.css'
import './App.polish.css'

const FRAME_WIDTH = 160
const FRAME_HEIGHT = 120

type ConnectionState = 'disconnected' | 'connecting' | 'connected'
type MotorStatus = 'IDLE' | 'FORWARD' | 'REVERSE'
type MotorDirection = 'forward' | 'reverse'

type MotorTelemetry = {
  state?: string
  moving?: boolean
  target?: number
  direction?: 'forward' | 'reverse' | 'stopped'
  speed?: number
  cleaning?: boolean
  cleanStep?: number
  home?: number
  halls?: boolean[]
  rssi?: number
}

type Telemetry = {
  type?: string
  status?: string
  halls?: boolean[]
  safety?: {
    topCover?: boolean
    drawer?: boolean
    pir?: boolean
    irUp?: boolean
    irDown?: boolean
  }
  binFull?: boolean
  weight?: number
  temp?: number
  humidity?: number
  tof?: number
  camera?: {
    width?: number
    height?: number
    frames?: number
    dropped?: number
    bytes?: number
    format?: string
  }
}

function defaultDeviceHost() {
  const saved = localStorage.getItem('catLitterDeviceHost')
  if (saved) return saved

  const configured = import.meta.env.VITE_ESP32_HOST as string | undefined
  if (configured) return configured

  return ['localhost', '127.0.0.1'].includes(window.location.hostname)
    ? ''
    : window.location.hostname
}

function defaultMotorHost() {
  const saved = localStorage.getItem('catLitterMotorHost')
  if (saved) return saved
  return (import.meta.env.VITE_MOTOR_ESP32_HOST as string | undefined) ?? ''
}

function normalizeHost(value: string) {
  return value
    .trim()
    .replace(/^https?:\/\//, '')
    .replace(/\/.*$/, '')
    .replace(/:80$/, '')
}

function drawJpeg(canvas: HTMLCanvasElement, buffer: ArrayBuffer) {
  return new Promise<boolean>((resolve) => {
    const context = canvas.getContext('2d')
    if (!context || buffer.byteLength < 4) {
      resolve(false)
      return
    }

    const objectUrl = URL.createObjectURL(new Blob([buffer], { type: 'image/jpeg' }))
    const image = new Image()
    image.onload = () => {
      canvas.width = image.naturalWidth
      canvas.height = image.naturalHeight
      context.drawImage(image, 0, 0)
      URL.revokeObjectURL(objectUrl)
      resolve(true)
    }
    image.onerror = () => {
      URL.revokeObjectURL(objectUrl)
      resolve(false)
    }
    image.src = objectUrl
  })
}

function formatMachineStatus(status: string) {
  const position = status.match(/^AT_H([1-4])$/)
  if (position) return `已定位到 H${position[1]}`

  const forward = status.match(/^FORWARD_TO_H([1-4])$/)
  if (forward) return `正向定位至 H${forward[1]}`

  const reverse = status.match(/^REVERSE_TO_H([1-4])$/)
  if (reverse) return `反向定位至 H${reverse[1]}`

  const labels: Record<string, string> = {
    IDLE: '系统待机',
    STOPPED: '电机已停止',
    EMERGENCY_STOP: '紧急停止',
    CLEAN_STARTING_AT_H3: '准备开始清理',
    CLEAN_PAUSE: '清理步骤切换中',
    CLEAN_COMPLETE_AT_H3: '清理完成',
    CLEAN_REQUIRES_H3: '请先回到原点 H3',
  }
  return labels[status] ?? status.replace(/_/g, ' ')
}

function App() {
  const canvasRef = useRef<HTMLCanvasElement>(null)
  const lastFetchedFrameRef = useRef(-1)
  const framesInWindowRef = useRef(0)
  const fpsWindowStartedRef = useRef(performance.now())

  const initialHost = defaultDeviceHost()
  const [deviceHost, setDeviceHost] = useState(initialHost)
  const [activeHost, setActiveHost] = useState(initialHost)
  const [connection, setConnection] = useState<ConnectionState>('disconnected')
  const [connectionError, setConnectionError] = useState('')
  const [hasFrame, setHasFrame] = useState(false)
  const [fps, setFps] = useState(0)
  const [lastFrameTime, setLastFrameTime] = useState('—')

  const initialMotorHost = defaultMotorHost()
  const [motorHost, setMotorHost] = useState(initialMotorHost)
  const [activeMotorHost, setActiveMotorHost] = useState(initialMotorHost)
  const [motorConnection, setMotorConnection] = useState<ConnectionState>('disconnected')
  const [motorError, setMotorError] = useState('')
  const [halls, setHalls] = useState({ h1: false, h2: false, h3: false, h4: false })
  const [motorStatus, setMotorStatus] = useState<MotorStatus>('IDLE')
  const [motorDirection, setMotorDirection] = useState<MotorDirection>('forward')
  const [motorSpeed, setMotorSpeed] = useState(200)
  const [cleaning, setCleaning] = useState(false)
  const [cleanStep, setCleanStep] = useState(0)
  const [machineStatus, setMachineStatus] = useState('未知')
  const [weight, setWeight] = useState(0)
  const [tofDistance, setTofDistance] = useState(0)
  const [temp, setTemp] = useState(0)
  const [humidity, setHumidity] = useState(0)
  const [topCoverOpen, setTopCoverOpen] = useState(false)
  const [drawerOpen, setDrawerOpen] = useState(false)
  const [pirMotion, setPirMotion] = useState(false)
  const [irTopBlocked, setIrTopBlocked] = useState(false)
  const [irBottomBlocked, setIrBottomBlocked] = useState(false)
  const [irWasteFull, setIrWasteFull] = useState(false)

  useEffect(() => {
    if (!activeHost) {
      setConnection('disconnected')
      setConnectionError('请输入 ESP32-S3 串口显示的 IP 地址')
      return
    }

    let disposed = false
    let pollTimer: number | undefined

    const applyTelemetry = (data: Telemetry) => {
      if (data.weight !== undefined) setWeight(data.weight)
      if (data.tof !== undefined) setTofDistance(data.tof)
      if (data.temp !== undefined) setTemp(data.temp)
      if (data.humidity !== undefined) setHumidity(data.humidity)
      if (data.safety?.topCover !== undefined) setTopCoverOpen(data.safety.topCover)
      if (data.safety?.drawer !== undefined) setDrawerOpen(data.safety.drawer)
      if (data.safety?.pir !== undefined) setPirMotion(data.safety.pir)
      if (data.safety?.irUp !== undefined) setIrTopBlocked(data.safety.irUp)
      if (data.safety?.irDown !== undefined) setIrBottomBlocked(data.safety.irDown)
      if (data.binFull !== undefined) setIrWasteFull(data.binFull)
    }

    const poll = async () => {
      if (disposed) return
      try {
        const statusResponse = await fetch(`http://${activeHost}/status`, {
          cache: 'no-store',
        })
        if (!statusResponse.ok) throw new Error(`HTTP ${statusResponse.status}`)
        const telemetry = await statusResponse.json() as Telemetry
        applyTelemetry(telemetry)
        setConnection('connected')
        setConnectionError('')

        const frameNumber = telemetry.camera?.frames
        if (frameNumber !== undefined && frameNumber !== lastFetchedFrameRef.current) {
          const frameResponse = await fetch(`http://${activeHost}/frame?t=${frameNumber}`, {
            cache: 'no-store',
          })
          if (frameResponse.ok && canvasRef.current) {
            const frame = await frameResponse.arrayBuffer()
            if (await drawJpeg(canvasRef.current, frame)) {
              lastFetchedFrameRef.current = frameNumber
              setHasFrame(true)
              setLastFrameTime(new Date().toLocaleTimeString())
              framesInWindowRef.current += 1

              const now = performance.now()
              const elapsed = now - fpsWindowStartedRef.current
              if (elapsed >= 1000) {
                setFps(Math.round((framesInWindowRef.current * 10000) / elapsed) / 10)
                framesInWindowRef.current = 0
                fpsWindowStartedRef.current = now
              }
            }
          }
        }
      } catch {
        setConnection('disconnected')
        setConnectionError(`无法连接 http://${activeHost}`)
      }

      if (!disposed) pollTimer = window.setTimeout(poll, 250)
    }

    setConnection('connecting')
    lastFetchedFrameRef.current = -1
    poll()
    return () => {
      disposed = true
      if (pollTimer !== undefined) window.clearTimeout(pollTimer)
    }
  }, [activeHost])

  useEffect(() => {
    if (!activeMotorHost) {
      setMotorConnection('disconnected')
      return
    }

    let disposed = false
    let timer: number | undefined

    const pollMotor = async () => {
      if (disposed) return
      try {
        const response = await fetch(`http://${activeMotorHost}/status`, {
          cache: 'no-store',
        })
        if (!response.ok) throw new Error(`HTTP ${response.status}`)
        const data = await response.json() as MotorTelemetry
        if (data.halls?.length === 4) {
          setHalls({
            h1: Boolean(data.halls[0]),
            h2: Boolean(data.halls[1]),
            h3: Boolean(data.halls[2]),
            h4: Boolean(data.halls[3]),
          })
        }
        setMotorStatus(data.moving
          ? data.direction === 'reverse' ? 'REVERSE' : 'FORWARD'
          : 'IDLE')
        setCleaning(Boolean(data.cleaning))
        setCleanStep(data.cleanStep ?? 0)
        setMachineStatus(data.state ?? '未知')
        setMotorConnection('connected')
        setMotorError('')
      } catch {
        setMotorConnection('disconnected')
        setMotorStatus('IDLE')
        setMotorError(`无法连接电机控制器 http://${activeMotorHost}`)
      }
      if (!disposed) timer = window.setTimeout(pollMotor, 250)
    }

    setMotorConnection('connecting')
    pollMotor()
    return () => {
      disposed = true
      if (timer !== undefined) window.clearTimeout(timer)
    }
  }, [activeMotorHost])

  useEffect(() => {
    if (motorConnection !== 'connected' || !activeMotorHost) return

    const timer = window.setTimeout(async () => {
      try {
        const response = await fetch(
          `http://${activeMotorHost}/speed?value=${motorSpeed}`,
          { cache: 'no-store' },
        )
        if (!response.ok) throw new Error(`HTTP ${response.status}`)
        setMotorError('')
      } catch {
        setMotorError('速度设置失败')
      }
    }, 150)

    return () => window.clearTimeout(timer)
  }, [activeMotorHost, motorConnection, motorSpeed])

  const connectToDevice = (event: FormEvent) => {
    event.preventDefault()
    const normalized = normalizeHost(deviceHost)
    setDeviceHost(normalized)
    localStorage.setItem('catLitterDeviceHost', normalized)
    setActiveHost(normalized)
  }

  const connectToMotor = (event: FormEvent) => {
    event.preventDefault()
    const normalized = normalizeHost(motorHost)
    setMotorHost(normalized)
    localStorage.setItem('catLitterMotorHost', normalized)
    setActiveMotorHost(normalized)
  }

  const sendMotorCommand = async (target: 'H1' | 'H2' | 'H3' | 'H4' | 'STOP') => {
    if (motorConnection !== 'connected' || !activeMotorHost) {
      setMotorError('电机控制器未连接，无法发送指令')
      return
    }
    try {
      const path = target === 'STOP'
        ? '/stop'
        : `/move?target=${encodeURIComponent(target)}&direction=${motorDirection}`
      const response = await fetch(`http://${activeMotorHost}${path}`, { cache: 'no-store' })
      if (!response.ok) throw new Error(`HTTP ${response.status}`)
    } catch {
      setMotorError('电机指令发送失败')
    }
  }

  const startCleaning = async () => {
    if (motorConnection !== 'connected' || !activeMotorHost) {
      setMotorError('电机控制器未连接，无法开始清理')
      return
    }
    try {
      const response = await fetch(`http://${activeMotorHost}/clean`, { cache: 'no-store' })
      if (!response.ok) {
        const result = await response.json() as { error?: string }
        throw new Error(result.error ?? `HTTP ${response.status}`)
      }
      setMotorError('')
    } catch (error) {
      setMotorError(error instanceof Error && error.message.includes('H3')
        ? '一键清理必须从原点 H3 开始'
        : '一键清理启动失败')
    }
  }

  const connectionLabel = connection === 'connected'
    ? '设备在线'
    : connection === 'connecting'
      ? '正在连接'
      : '设备离线'

  const activeHall = halls.h1 ? 'H1' : halls.h2 ? 'H2' : halls.h3 ? 'H3' : halls.h4 ? 'H4' : '—'
  const readableMachineStatus = formatMachineStatus(machineStatus)

  return (
    <div className="gh-layout">
      <header className="gh-header">
        <div className="brand-lockup">
          <div className="brand-mark">CL</div>
          <div>
            <div className="gh-header-logo">智能猫砂盆控制中枢</div>
            <div className="brand-subtitle">CAT LITTER AUTOMATION CONSOLE</div>
          </div>
        </div>
        <div className="header-statuses">
          <div className={`header-device-status ${connection}`}>
            <span className="status-pulse"></span>
            <span><small>摄像系统</small>{connectionLabel}</span>
          </div>
          <div className={`header-device-status ${motorConnection}`}>
            <span className="status-pulse"></span>
            <span><small>运动系统</small>{motorConnection === 'connected' ? '设备在线' : '设备离线'}</span>
          </div>
        </div>
      </header>

      <main className="gh-main">
        <section className="connection-panel" aria-label="设备连接">
          <div className="connection-panel-heading">
            <span className="eyebrow">DEVICE NETWORK</span>
            <strong>设备连接</strong>
          </div>
          <form className="device-connect" onSubmit={connectToDevice}>
            <div className="connection-kind camera-kind"><span>CAM</span></div>
            <div className="connection-fields">
              <label htmlFor="device-host">摄像头 S3</label>
              <input id="device-host" value={deviceHost} onChange={(event) => setDeviceHost(event.target.value)} placeholder="192.168.1.121" />
            </div>
            <button className="gh-btn connection-button" type="submit">连接</button>
            {connectionError && <span className="connection-error">{connectionError}</span>}
          </form>

          <form className="device-connect" onSubmit={connectToMotor}>
            <div className="connection-kind motor-kind"><span>MTR</span></div>
            <div className="connection-fields">
              <label htmlFor="motor-host">电机 S3</label>
              <input id="motor-host" value={motorHost} onChange={(event) => setMotorHost(event.target.value)} placeholder="第二块 S3 的 IP" />
            </div>
            <button className="gh-btn connection-button" type="submit">连接</button>
            {motorError && <span className="connection-error">{motorError}</span>}
          </form>
        </section>

        <div className="gh-grid-top">
          <div className="gh-box camera-box">
            <div className="gh-box-header">
              <span className="gh-box-title"><small>L12 · LIVE VIEW</small>内部监控画面</span>
              <span className={`gh-badge ${connection === 'connected' && hasFrame ? 'active' : ''}`}>
                <span className="dot"></span>
                {connection === 'connected' && hasFrame ? '直播中' : '等待画面'}
              </span>
            </div>
            <div className="gh-box-body camera-body">
              <div className="camera-feed">
                <canvas
                  ref={canvasRef}
                  className={hasFrame ? 'camera-canvas visible' : 'camera-canvas'}
                  width={FRAME_WIDTH}
                  height={FRAME_HEIGHT}
                />
                {!hasFrame && <span className="camera-text">无视频流信号 / 摄像头未就绪</span>}
                <div className="camera-overlay-stats">
                  <span>帧率: {fps.toFixed(1)} fps</span>
                  <span>分辨率: {FRAME_WIDTH}×{FRAME_HEIGHT} JPEG</span>
                  <span>最后画面: {lastFrameTime}</span>
                </div>
              </div>
            </div>
          </div>

          <div className="gh-box core-box" style={{ display: 'flex', flexDirection: 'column' }}>
            <div className="gh-box-header">
              <span className="gh-box-title"><small>MOTION CONTROL</small>滚筒驱动中心</span>
            </div>
            <div className="gh-box-body" style={{ display: 'flex', flexDirection: 'column', gap: '24px', flex: 1 }}>
              <div>
                <div className="section-label">滚筒霍尔定位</div>
                <button
                  className="gh-btn gh-btn-primary clean-cycle-button"
                  disabled={motorConnection !== 'connected' || cleaning || motorStatus !== 'IDLE'}
                  onClick={startCleaning}
                >{cleaning ? '清理流程运行中…' : '一键清理'}</button>
                <div className="clean-cycle-route">
                  {['反向 H2', '正向 H4', '反向 H3'].map((label, index) => (
                    <span key={label} className={cleaning && index === cleanStep ? 'route-active' : index < cleanStep ? 'route-done' : ''}>
                      <b>{index + 1}</b>{label}
                    </span>
                  ))}
                </div>
                <div className="speed-control">
                  <div className="speed-control-label">
                    <label htmlFor="motor-speed">电机速度</label>
                    <output htmlFor="motor-speed">PWM {motorSpeed}</output>
                  </div>
                  <input
                    id="motor-speed"
                    type="range"
                    min="30"
                    max="255"
                    step="1"
                    value={motorSpeed}
                    disabled={motorConnection !== 'connected'}
                    onChange={(event) => setMotorSpeed(Number(event.target.value))}
                  />
                  <div className="speed-scale"><span>慢 30</span><span>快 255</span></div>
                </div>
                <div className="direction-selector" role="group" aria-label="电机定位方向">
                  <button
                    className={`gh-btn ${motorDirection === 'forward' ? 'direction-active' : ''}`}
                    onClick={() => setMotorDirection('forward')}
                  >正向定位</button>
                  <button
                    className={`gh-btn ${motorDirection === 'reverse' ? 'direction-active' : ''}`}
                    onClick={() => setMotorDirection('reverse')}
                  >反向定位</button>
                </div>
                <div className="command-grid">
                  <button className="gh-btn" disabled={motorConnection !== 'connected' || cleaning} onClick={() => sendMotorCommand('H1')}>转到 H1</button>
                  <button className="gh-btn" disabled={motorConnection !== 'connected' || cleaning} onClick={() => sendMotorCommand('H2')}>转到 H2</button>
                  <button className="gh-btn gh-btn-primary" disabled={motorConnection !== 'connected' || cleaning} onClick={() => sendMotorCommand('H3')}>转到 H3（原点）</button>
                  <button className="gh-btn" disabled={motorConnection !== 'connected' || cleaning} onClick={() => sendMotorCommand('H4')}>转到 H4</button>
                  <button className="gh-btn gh-btn-danger command-stop" disabled={motorConnection !== 'connected'} onClick={() => sendMotorCommand('STOP')}>紧急停止</button>
                </div>
              </div>

              <hr className="gh-divider" />

              <div className="gh-motor-area">
                <div className={`gh-motor-spin ${motorStatus === 'FORWARD' ? 'fwd' : motorStatus === 'REVERSE' ? 'rev' : ''}`}>
                  <div className="gh-motor-text">{activeHall}</div>
                </div>
                <div className="machine-state-copy">
                  <small>当前运行状态</small>
                  <div className="machine-status">{readableMachineStatus}</div>
                  <div className="machine-state-meta">
                    <span>当前位置 <b>{activeHall}</b></span>
                    <span>速度 <b>PWM {motorSpeed}</b></span>
                  </div>
                </div>
                <span className={`motor-state-indicator ${motorStatus.toLowerCase()}`}>
                  {motorStatus === 'FORWARD' ? '正转' : motorStatus === 'REVERSE' ? '反转' : '静止'}
                </span>
              </div>

              <div className="hall-position-panel">
                <div className="hall-position-heading">
                  <div className="section-label">L17 滚筒绝对定位编码器</div>
                  <span>LOW = 已触发</span>
                </div>
                <div className="gh-status-row">
                  <div className={`gh-badge ${halls.h1 ? 'active' : ''}`}><span className="dot"></span> H1</div>
                  <div className={`gh-badge ${halls.h2 ? 'active' : ''}`}><span className="dot"></span> H2</div>
                  <div className={`gh-badge ${halls.h3 ? 'active' : ''}`}><span className="dot"></span> H3 (原点)</div>
                  <div className={`gh-badge ${halls.h4 ? 'active' : ''}`}><span className="dot"></span> H4</div>
                </div>
              </div>
            </div>
          </div>
        </div>

        <div className="gh-grid-3">
          <div className="gh-box">
            <div className="gh-box-header"><span className="gh-box-title"><small>ENVIRONMENT</small>环境与健康指标</span></div>
            <div className="gh-box-body">
              <ul className="gh-list">
                <li><span className="label">L2 称重</span><span className="value highlight-blue">{weight.toFixed(2)} kg</span></li>
                <li><span className="label">TOF 激光测距</span><span className="value">{tofDistance || '—'} mm</span></li>
                <li><span className="label">L3 舱内温度</span><span className="value">{temp || '—'} °C</span></li>
                <li><span className="label">L3 舱内湿度</span><span className="value">{humidity || '—'} %</span></li>
              </ul>
            </div>
          </div>

          <div className="gh-box">
            <div className="gh-box-header"><span className="gh-box-title"><small>SAFETY</small>安全与物理保护</span></div>
            <div className="gh-box-body">
              <ul className="gh-list">
                <li><span className="label">L7 顶盖状态</span><span className={`gh-badge ${topCoverOpen ? 'danger' : ''}`}><span className="dot"></span>{topCoverOpen ? '被掀开' : '未接入'}</span></li>
                <li><span className="label">L4 集便抽屉</span><span className={`gh-badge ${drawerOpen ? 'danger' : ''}`}><span className="dot"></span>{drawerOpen ? '已拉出' : '未接入'}</span></li>
                <li><span className="label">L14 热释电</span><span className={`gh-badge ${pirMotion ? 'warn' : ''}`}><span className="dot"></span>{pirMotion ? '检测到移动' : '未接入'}</span></li>
              </ul>
            </div>
          </div>

          <div className="gh-box">
            <div className="gh-box-header"><span className="gh-box-title"><small>OPTICAL ARRAY</small>光电对射检测</span></div>
            <div className="gh-box-body">
              <ul className="gh-list">
                <li><span className="label">顶部防夹光栅</span><span className={`gh-badge ${irTopBlocked ? 'warn' : 'active'}`}><span className="dot"></span>{irTopBlocked ? '有异物遮挡' : '通畅'}</span></li>
                <li><span className="label">底部防夹光栅</span><span className={`gh-badge ${irBottomBlocked ? 'warn' : 'active'}`}><span className="dot"></span>{irBottomBlocked ? '有异物遮挡' : '通畅'}</span></li>
                <li><span className="label">集便盆满载</span><span className={`gh-badge ${irWasteFull ? 'danger' : 'active'}`}><span className="dot"></span>{irWasteFull ? '容量已满' : '未满'}</span></li>
              </ul>
            </div>
          </div>
        </div>
      </main>
    </div>
  )
}

export default App
