Import("env")

env.AddCustomTarget(
    name="j3upload",
    dependencies=None,
    actions=[
        "platformio run -j 3 --target upload",
    ],
    title="j3upload",
    description="Upload (max 3 jobs)"
)
