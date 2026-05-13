import { useCallback, useContext, useEffect, useLayoutEffect, useRef, useState } from "react";
import { useLocation } from "preact-iso";
import { AppContext } from "../../index";
import "./style.css";

interface Valve {
  id: string;
  mode: 'on' | 'off' | 'auto';
  active: boolean;
  on: boolean;
}

interface ImStatus {
  error?: string;
  rssi?: number;
  heapAfterSetup?: number;
  heap?: number;
  waterLevel?: string;
  waterPressure?: number;
  waterPressureAdc?: number;
  irrigationPump: 'active' | 'inactive' | 'out-of-water';
  irrigationPumpMode: 'off' | 'auto';
  wellPump: 'active-cycle' | 'inactive-cycle' | 'inactive';
  wellPumpMode: 'on' | 'off' | 'auto';
  wellPumpCycle?: number;
  valves?: Valve[];
}

interface ScheduleSequence {
  duration: number;
  startTime?: string;
  valves: string[];
}

interface ScheduleCycle {
  start: string;
  end: string;
  active: boolean;
  area: {
    name: string;
    sequences: ScheduleSequence[];
  };
}

const formatTime = (time: string) => time.substring(0, 2) + ':' + time.substring(2);

const Status = ({}) => {
  const { setPageTitle } = useContext(AppContext);
  const { route } = useLocation();
  useLayoutEffect(() => {
    setPageTitle('Status');
  });

  const [ currentDate, setCurrentDate ] = useState<Date | undefined>(undefined);
  const currentDateRef = useRef<number>(0);
  const timerRef = useRef<number>(-1);
  const [ status, setStatus ] = useState<ImStatus>({
    irrigationPump: "inactive",
    irrigationPumpMode: "auto",
    wellPump: "inactive",
    wellPumpMode: "auto",
  });

  const [ pendingWellPump, setPendingWellPump ] = useState(false);
  const [ pendingIrrigationPump, setPendingIrrigationPump ] = useState(false);
  const [ pendingValves, setPendingValves ] = useState<Set<number>>(new Set());

  const [ connected, setConnected ] = useState(false);
  const eventSource = useRef<EventSource | undefined>(undefined);
  useEffect(() => {
    let cancelled = false;
    let reconnectTimer: ReturnType<typeof setTimeout> | undefined;

    const connect = () => {
      if (cancelled) return;
      const es = new EventSource('/api/status-events');
      eventSource.current = es;

      es.onopen = () => {
        console.log("Events Connected");
        setConnected(true);
      };
      es.onerror = () => {
        if (es.readyState !== EventSource.OPEN) {
          console.log("Events Disconnected");
          setConnected(false);
          es.close();
          if (eventSource.current === es) eventSource.current = undefined;
          if (!cancelled && reconnectTimer === undefined) {
            reconnectTimer = setTimeout(() => { reconnectTimer = undefined; connect(); }, 2000);
          }
        } else {
          console.error("Event Source error");
        }
      };
      es.addEventListener('INIT', event => {
        const data = JSON.parse((event as MessageEvent).data);
        setStatus(data);
        if (data['currentDate']) {
          currentDateRef.current = new Date(data['currentDate']).getTime();
        }
      });
      es.addEventListener('UPDATE', event => {
        const data = JSON.parse((event as MessageEvent).data);
        setStatus(prev => ({ ...prev, ...data }));
        if (data['currentDate']) {
          currentDateRef.current = new Date(data['currentDate']).getTime();
        }
        if (data['wellPumpMode'] !== undefined) setPendingWellPump(false);
        if (data['irrigationPumpMode'] !== undefined) setPendingIrrigationPump(false);
        if (data['valves'] !== undefined) { setPendingValves(new Set()); setPendingAllValves(false); }
      });
    };

    const handleVisibilityChange = () => {
      if (document.visibilityState !== 'visible') return;
      const es = eventSource.current;
      if (!es || es.readyState !== EventSource.OPEN) {
        if (es) es.close();
        eventSource.current = undefined;
        if (reconnectTimer !== undefined) {
          clearTimeout(reconnectTimer);
          reconnectTimer = undefined;
        }
        setConnected(false);
        connect();
      }
    };

    connect();
    document.addEventListener('visibilitychange', handleVisibilityChange);

    timerRef.current = setInterval(() => {
      if (currentDateRef.current === 0) return;
      currentDateRef.current += 1000;
      setCurrentDate(new Date(currentDateRef.current));
    }, 1000);
    return () => {
      cancelled = true;
      document.removeEventListener('visibilitychange', handleVisibilityChange);
      if (reconnectTimer !== undefined) clearTimeout(reconnectTimer);
      if (timerRef.current !== -1) {
        clearInterval(timerRef.current);
      }
      setConnected(false);
      setCurrentDate(undefined);
      setStatus(undefined);
      if (eventSource.current) eventSource.current.close();
      eventSource.current = undefined;
    }
  }, []);

  const setWellPumpMode = (mode: 'on' | 'off' | 'auto') => {
    setPendingWellPump(true);
    fetch('/api/well-pump', {
      method: 'POST',
      headers: new Headers({ 'Content-Type': 'application/x-www-form-urlencoded' }),
      body: `mode=${mode}`
    }).catch(error => { console.log(error); setPendingWellPump(false); });
  };

  const setIrrigationPumpMode = (mode: 'off' | 'auto') => {
    setPendingIrrigationPump(true);
    fetch('/api/irrigation-pump', {
      method: 'POST',
      headers: new Headers({ 'Content-Type': 'application/x-www-form-urlencoded' }),
      body: `mode=${mode}`
    }).catch(error => { console.log(error); setPendingIrrigationPump(false); });
  };

  const setValveMode = (index: number, mode: 'on' | 'off' | 'auto') => {
    setPendingValves(prev => new Set(prev).add(index));
    fetch(`/api/irrigation/valve`, {
      method: 'POST',
      headers: new Headers({ 'Content-Type': 'application/x-www-form-urlencoded' }),
      body: `index=${index}&mode=${mode}`
    }).catch(error => { console.log(error); setPendingValves(prev => { const next = new Set(prev); next.delete(index); return next; }); });
  };

  const [ scheduleCycles, setScheduleCycles ] = useState<ScheduleCycle[]>([]);
  useEffect(() => {
    if (!connected) return;
    fetch('/api/irrigation/schedule')
        .then(res => res.json())
        .then(data => setScheduleCycles(data.cycles || []))
        .catch(() => {});
  }, [ connected ]);

  const [ pendingAllValves, setPendingAllValves ] = useState(false);
  const setAllValvesMode = (mode: 'off' | 'auto') => {
    setPendingAllValves(true);
    fetch(`/api/irrigation/valve`, {
      method: 'POST',
      headers: new Headers({ 'Content-Type': 'application/x-www-form-urlencoded' }),
      body: `mode=${mode}`
    }).catch(error => { console.log(error); setPendingAllValves(false); });
  };

  const freeHeap = status.heapAfterSetup === undefined || status.heap == undefined
      ? undefined
      : status.heapAfterSetup - status.heap;
  return (
      <div className="status-main">
        {
          connected
              ? <table className="status-table">
                <tbody>
                <tr>
                  <td style={ {  verticalAlign: 'top' } }>
                    System:
                  </td>
                  <td>
                    <table>
                    <tbody>
                      <tr>
                        <td>
                          <div>
                            {
                              currentDate === undefined
                                  ? 'Waiting for NTP response'
                                  : currentDate.toLocaleString()
                            }
                          </div>
                        </td>
                      </tr>
                      <tr>
                        <td>
                          <div>
                            RSSI:&nbsp;
                            {
                              status.rssi === undefined
                                  ? 'No RSSI given'
                                  : status.rssi
                            }
                          </div>
                        </td>
                      </tr>
                      <tr>
                        <td>
                          <div>
                            Mem:&nbsp;
                            {
                              freeHeap === undefined
                                  ? 'No heap information'
                                  : <span>{
                                        (freeHeap * 100 / status.heapAfterSetup).toFixed(2)
                                      }% used<br />
                                      ({
                                        freeHeap
                                      } bytes of {
                                        status.heapAfterSetup
                                      })
                                    </span>
                            }
                          </div>
                        </td>
                      </tr>
                      {
                        status.error == undefined
                            ? undefined
                            : <tr>
                                <td style="color: red; font-weight: bold;">
                                  <div>
                                    Error:
                                    {
                                      status.error
                                    }
                                  </div>
                                </td>
                              </tr>
                      }
                    </tbody>
                    </table>
                  </td>
                </tr>
                <tr>
                  <td>Level:</td>
                  <td>
                    <div>
                      {
                        status.waterLevel
                      }
                    </div>
                  </td>
                </tr>
                <tr>
                  <td>Pressure:</td>
                  <td>
                    <div>
                      {
                        status.waterPressure?.toFixed(2)
                      }bar (ADC: {
                        status.waterPressureAdc
                      })
                    </div>
                  </td>
                </tr>
                <tr>
                  <td>Well-P.:</td>
                  <td>
                    <div>
                      <div>
                        {
                          status.wellPump === "active-cycle"
                              ? <div className="led-off led-blinking-yellow"></div>
                              : status.wellPump === "inactive-cycle"
                                  ? <div className="led-off led-flashing-yellow"></div>
                                  : <div className="led-off"></div>
                        }
                        &nbsp;
                        {
                          status.wellPump === "active-cycle"
                              ? `active ${ status.wellPumpCycle === 0 ? '' : `(${status.wellPumpCycle} mins)` }`
                              : status.wellPump === "inactive-cycle"
                                  ? `inactive ${ status.wellPumpCycle === 0 ? '' : `(${status.wellPumpCycle} mins)` }`
                                  : "off"
                        }
                      </div>
                      <div>
                        <button
                            disabled={ pendingWellPump }
                            style={ status.wellPumpMode === 'on' ? { backgroundColor: 'grey', color: 'white' } : undefined }
                            onClick={ () => setWellPumpMode('on') }>
                          1
                        </button>
                        <button
                            disabled={ pendingWellPump }
                            style={ status.wellPumpMode === 'auto' ? { backgroundColor: 'grey', color: 'white' } : undefined }
                            onClick={ () => setWellPumpMode('auto') }>
                          A
                        </button>
                        <button
                            disabled={ pendingWellPump }
                            style={ status.wellPumpMode === 'off' ? { backgroundColor: 'grey', color: 'white' } : undefined }
                            onClick={ () => setWellPumpMode('off') }>
                          0
                        </button>
                      </div>
                    </div>
                  </td>
                </tr>
                <tr>
                  <td>Irrigation-P.:</td>
                  <td>
                    <div>
                      <div>
                        {
                          status.irrigationPump === "active"
                              ? <div className="led-off led-blinking-red"></div>
                              : status.irrigationPump === "out-of-water"
                                  ? <div className="led-off led-blinking-fast-red"></div>
                                  : <div className="led-off"></div>
                        }
                        &nbsp;
                        {
                          status.irrigationPump === "active"
                              ? "active"
                              : status.irrigationPump === "out-of-water"
                                  ? "inactive"
                                  : "off"
                        }
                      </div>
                      <div>
                        <button
                            disabled={ pendingIrrigationPump }
                            style={ status.irrigationPumpMode === 'auto'
                                ? { backgroundColor: 'grey', color: 'white' }
                                : undefined }
                            onClick={ () => setIrrigationPumpMode('auto') }>
                          A
                        </button>
                        <button
                            disabled={ pendingIrrigationPump }
                            style={ status.irrigationPumpMode === 'off'
                                ? { backgroundColor: 'grey', color: 'white' }
                                : undefined }
                            onClick={ () => setIrrigationPumpMode('off') }>
                          0
                        </button>
                      </div>
                    </div>
                  </td>
                </tr>
                <tr>
                  <td style={ { verticalAlign: 'top' } }>
                    <a href="#" onClick={ (e: Event) => { e.preventDefault(); route('/schedule'); } }
                       style={ { color: 'inherit' } }>
                      Cycles:
                    </a>
                  </td>
                  <td>
                    {
                      (() => {
                        const activeCycles = scheduleCycles.filter(c => c.active);
                        if (activeCycles.length > 0) {
                          return activeCycles.map((cycle, i) => {
                            const activeSeq = cycle.area.sequences.find(s => s.startTime !== undefined);
                            return (
                                <div key={i}>
                                  <div>
                                    <div className="led-off led-blinking-green"></div>
                                    &nbsp;
                                    {cycle.area.name}
                                    {activeSeq ? ` (${activeSeq.valves.join(', ')})` : ''}
                                  </div>
                                </div>);
                          });
                        }
                        const nextCycle = scheduleCycles.find(c => !c.active);
                        if (nextCycle) {
                          return (
                              <div>
                                <div>
                                  <div className="led-off led-grey"></div>
                                  &nbsp;
                                  {nextCycle.area.name} @ {formatTime(nextCycle.start)}
                                </div>
                              </div>);
                        }
                        return <div><div>No active cycles</div></div>;
                      })()
                    }
                  </td>
                </tr>
                <tr>
                  <td>Valves:</td>
                  <td>
                    <div>
                      <div>
                        All
                      </div>
                      <div>
                        <button
                            disabled={ pendingAllValves }
                            style={ status.valves?.every(v => v.mode === 'auto') ? { backgroundColor: 'grey', color: 'white' } : undefined }
                            onClick={ () => setAllValvesMode('auto') }>
                          A
                        </button>
                        <button
                            disabled={ pendingAllValves }
                            style={ status.valves?.every(v => v.mode === 'off') ? { backgroundColor: 'grey', color: 'white' } : undefined }
                            onClick={ () => setAllValvesMode('off') }>
                          0
                        </button>
                      </div>
                    </div>
                  </td>
                </tr>
                <tr>
                  <td></td>
                  <td>
                    {
                      status.valves?.map((valve, valveIndex) =>
                          <div>
                            <div>
                              {
                                valve.on
                                    ? <div className="led-off led-blinking-blue"></div>
                                    : <div className="led-off led-grey"></div>
                              }
                              &nbsp;
                              {
                                valve.id
                              }
                              :&nbsp;
                              {
                                valve.on
                                    ? "on"
                                    : "off"
                              }
                            </div>
                            <div>
                              <button
                                  disabled={ pendingValves.has(valveIndex) }
                                  style={ valve.mode === 'on' ? { backgroundColor: 'grey', color: 'white' } : undefined }
                                  onClick={ () => setValveMode(valveIndex, 'on') }>
                                1
                              </button>
                              <button
                                  disabled={ pendingValves.has(valveIndex) }
                                  style={ valve.mode === 'auto' ? { backgroundColor: 'grey', color: 'white' } : undefined }
                                  onClick={ () => setValveMode(valveIndex, 'auto') }>
                                A
                              </button>
                              <button
                                  disabled={ pendingValves.has(valveIndex) }
                                  style={ valve.mode === 'off' ? { backgroundColor: 'grey', color: 'white' } : undefined }
                                  onClick={ () => setValveMode(valveIndex, 'off') }>
                                0
                              </button>
                            </div>
                          </div>)
                    }

                  </td>
                </tr>
                </tbody>
              </table>
              : <div>Connecting....</div>
        }
      </div>
  );
};

export { Status };
