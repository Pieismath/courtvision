using UnrealBuildTool;
public class CourtVision : ModuleRules {
 public CourtVision(ReadOnlyTargetRules Target) : base(Target) {
  PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
  PublicDependencyModuleNames.AddRange(new string[]{"Core","CoreUObject","Engine","InputCore","Sockets","Networking","Json","Slate","SlateCore","UMG","RenderCore"});
 }
}
