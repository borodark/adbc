Adbc.download_driver!(:sqlite)
Adbc.download_driver!(:duckdb)

pg_exclude =
  if System.find_executable("psql") do
    Adbc.download_driver!(:postgresql)
    []
  else
    [:postgresql]
  end

windows_exclude =
  case :os.type() do
    {:win32, _} -> [:unix]
    _ -> []
  end

# Cube tests are excluded by default since they require a running cubesqld server
# Run with: mix test --include cube
# Or use: test/run_cube_tests.sh
cube_exclude = [:cube]

ExUnit.start(exclude: pg_exclude ++ windows_exclude ++ cube_exclude)
