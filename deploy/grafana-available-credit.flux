// Available Grid Credit (GridTokenReader): last meter reading (kWh, camera or
// manual) minus grid energy drawn since then (Clematis MCB power integral).
import "array"
import "date"

price = 1785.7  // Rp per kWh (effective prepaid tariff)

a = from(bucket: "inverter")
  |> range(start: -90d)
  |> filter(fn: (r) => r._measurement == "pln_prepaid" and r._field == "kwh")
  |> group()
  |> sort(columns: ["_time"])
  |> last()
  |> findRecord(fn: (key) => true, idx: 0)

t0 = if exists a._time then a._time else date.sub(d: 1m, from: now())

used = from(bucket: "inverter")
  |> range(start: t0)
  |> filter(fn: (r) => r._measurement == "clematis_mcb_power" and r._field == "value")
  |> group()
  |> integral(unit: 1h)
  |> map(fn: (r) => ({_value: r._value / 1000.0}))

union(tables: [used, array.from(rows: [{_value: 0.0}])])
  |> sum()
  |> filter(fn: (r) => exists a._value)
  |> map(fn: (r) => ({_time: now(), _value: (a._value - r._value) * price}))
