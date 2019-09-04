Accepted a new connection on port 29000

;;;;;; IR Dump At START of cleanup ;;;;;;
source_filename = "basic/linear_hot.cpp"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@.str = private unnamed_addr constant [18 x i8] c"stoneSteps = %lu\0A\00", align 1

; Function Attrs: noinline uwtable
define dso_local i64 @_Z8fib_leftm(i64 %n) #0 {
entry:
  %retval = alloca i64, align 8
  %n.addr = alloca i64, align 8
  store i64 %n, i64* %n.addr, align 8, !tbaa !2
  %0 = load i64, i64* %n.addr, align 8, !tbaa !2
  %cmp = icmp ult i64 %0, 2
  br i1 %cmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  %1 = load i64, i64* %n.addr, align 8, !tbaa !2
  store i64 %1, i64* %retval, align 8
  br label %return

if.end:                                           ; preds = %entry
  %2 = load i64, i64* %n.addr, align 8, !tbaa !2
  %sub = sub i64 %2, 1
  %call = call i64 @_Z8fib_leftm(i64 %sub)
  %3 = load i64, i64* %n.addr, align 8, !tbaa !2
  %sub1 = sub i64 %3, 2
  %call2 = call i64 @_Z9fib_rightm(i64 %sub1)
  %add = add i64 %call, %call2
  store i64 %add, i64* %retval, align 8
  br label %return

return:                                           ; preds = %if.end, %if.then
  %4 = load i64, i64* %retval, align 8
  ret i64 %4
}

; Function Attrs: noinline uwtable
define dso_local i64 @_Z9fib_rightm(i64 %n) #0 {
entry:
  %retval = alloca i64, align 8
  %n.addr = alloca i64, align 8
  store i64 %n, i64* %n.addr, align 8, !tbaa !2
  %0 = load i64, i64* %n.addr, align 8, !tbaa !2
  %cmp = icmp ult i64 %0, 2
  br i1 %cmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  %1 = load i64, i64* %n.addr, align 8, !tbaa !2
  store i64 %1, i64* %retval, align 8
  br label %return

if.end:                                           ; preds = %entry
  %2 = load i64, i64* %n.addr, align 8, !tbaa !2
  %sub = sub i64 %2, 1
  %call = call i64 @_Z8fib_leftm(i64 %sub)
  %3 = load i64, i64* %n.addr, align 8, !tbaa !2
  %sub1 = sub i64 %3, 2
  %call2 = call i64 @_Z9fib_rightm(i64 %sub1)
  %add = add i64 %call, %call2
  store i64 %add, i64* %retval, align 8
  br label %return

return:                                           ; preds = %if.end, %if.then
  %4 = load i64, i64* %retval, align 8
  ret i64 %4
}

; Function Attrs: noinline uwtable
define dso_local i64 @_Z3fibm(i64 %n) #0 {
entry:
  %n.addr = alloca i64, align 8
  store i64 %n, i64* %n.addr, align 8, !tbaa !2
  %0 = load i64, i64* %n.addr, align 8, !tbaa !2
  %call = call i64 @_Z8fib_leftm(i64 %0)
  ret i64 %call
}

; Function Attrs: noinline nounwind uwtable
define dso_local i64 @_Z17compute_hailstonel(i64 %limit) #1 {
entry:
  %limit.addr = alloca i64, align 8
  %x = alloca i64, align 8
  %reachedOne = alloca i64, align 8
  %totalSteps = alloca i64, align 8
  store i64 %limit, i64* %limit.addr, align 8, !tbaa !2
  %0 = bitcast i64* %x to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* %0) #5
  store i64 27, i64* %x, align 8, !tbaa !2
  %1 = bitcast i64* %reachedOne to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* %1) #5
  store i64 0, i64* %reachedOne, align 8, !tbaa !2
  %2 = bitcast i64* %totalSteps to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* %2) #5
  store i64 0, i64* %totalSteps, align 8, !tbaa !2
  br label %while.cond

while.cond:                                       ; preds = %if.end6, %entry
  %3 = load i64, i64* %reachedOne, align 8, !tbaa !2
  %4 = load i64, i64* %limit.addr, align 8, !tbaa !2
  %cmp = icmp slt i64 %3, %4
  br i1 %cmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.cond
  %5 = load i64, i64* %x, align 8, !tbaa !2
  %cmp1 = icmp eq i64 %5, 1
  br i1 %cmp1, label %if.then, label %if.end

if.then:                                          ; preds = %while.body
  %6 = load i64, i64* %reachedOne, align 8, !tbaa !2
  %add = add nsw i64 27, %6
  store i64 %add, i64* %x, align 8, !tbaa !2
  %7 = load i64, i64* %reachedOne, align 8, !tbaa !2
  %inc = add nsw i64 %7, 1
  store i64 %inc, i64* %reachedOne, align 8, !tbaa !2
  br label %if.end

if.end:                                           ; preds = %if.then, %while.body
  %8 = load i64, i64* %totalSteps, align 8, !tbaa !2
  %inc2 = add nsw i64 %8, 1
  store i64 %inc2, i64* %totalSteps, align 8, !tbaa !2
  %9 = load i64, i64* %x, align 8, !tbaa !2
  %rem = srem i64 %9, 2
  %cmp3 = icmp eq i64 %rem, 0
  br i1 %cmp3, label %if.then4, label %if.else

if.then4:                                         ; preds = %if.end
  %10 = load i64, i64* %x, align 8, !tbaa !2
  %div = sdiv i64 %10, 2
  store i64 %div, i64* %x, align 8, !tbaa !2
  br label %if.end6

if.else:                                          ; preds = %if.end
  %11 = load i64, i64* %x, align 8, !tbaa !2
  %mul = mul nsw i64 3, %11
  %add5 = add nsw i64 %mul, 1
  store i64 %add5, i64* %x, align 8, !tbaa !2
  br label %if.end6

if.end6:                                          ; preds = %if.else, %if.then4
  br label %while.cond

while.end:                                        ; preds = %while.cond
  %12 = load i64, i64* %totalSteps, align 8, !tbaa !2
  %13 = bitcast i64* %totalSteps to i8*
  call void @llvm.lifetime.end.p0i8(i64 8, i8* %13) #5
  %14 = bitcast i64* %reachedOne to i8*
  call void @llvm.lifetime.end.p0i8(i64 8, i8* %14) #5
  %15 = bitcast i64* %x to i8*
  call void @llvm.lifetime.end.p0i8(i64 8, i8* %15) #5
  ret i64 %12
}

; Function Attrs: argmemonly nounwind willreturn
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #2

; Function Attrs: argmemonly nounwind willreturn
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #2

; Function Attrs: norecurse uwtable
define dso_local i32 @main() #3 {
entry:
  %retval = alloca i32, align 4
  %ans = alloca i64, align 8
  %stoneSteps = alloca i64, align 8
  %i = alloca i32, align 4
  %cleanup.dest.slot = alloca i32, align 4
  store i32 0, i32* %retval, align 4
  %0 = bitcast i64* %ans to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* %0) #5
  store i64 0, i64* %ans, align 8, !tbaa !2
  %1 = bitcast i64* %stoneSteps to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* %1) #5
  store i64 0, i64* %stoneSteps, align 8, !tbaa !2
  %2 = bitcast i32* %i to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* %2) #5
  store i32 -4, i32* %i, align 4, !tbaa !6
  br label %for.cond

for.cond:                                         ; preds = %for.inc, %entry
  %3 = load i32, i32* %i, align 4, !tbaa !6
  %cmp = icmp slt i32 %3, 10
  br i1 %cmp, label %for.body, label %for.end

for.body:                                         ; preds = %for.cond
  %4 = load i32, i32* %i, align 4, !tbaa !6
  %mul = mul nsw i32 500000, %4
  %conv = sext i32 %mul to i64
  %call = call i64 @_Z17compute_hailstonel(i64 %conv)
  %5 = load i64, i64* %stoneSteps, align 8, !tbaa !2
  %add = add i64 %5, %call
  store i64 %add, i64* %stoneSteps, align 8, !tbaa !2
  %call1 = call i64 @_Z3fibm(i64 40)
  %6 = load i64, i64* %ans, align 8, !tbaa !2
  %add2 = add i64 %6, %call1
  store i64 %add2, i64* %ans, align 8, !tbaa !2
  br label %for.inc

for.inc:                                          ; preds = %for.body
  %7 = load i32, i32* %i, align 4, !tbaa !6
  %inc = add nsw i32 %7, 1
  store i32 %inc, i32* %i, align 4, !tbaa !6
  br label %for.cond

for.end:                                          ; preds = %for.cond
  %8 = load i64, i64* %ans, align 8, !tbaa !2
  %9 = load i32, i32* %i, align 4, !tbaa !6
  %conv3 = sext i32 %9 to i64
  %rem = urem i64 %8, %conv3
  %cmp4 = icmp eq i64 %rem, 0
  br i1 %cmp4, label %if.then, label %if.end

if.then:                                          ; preds = %for.end
  store i32 0, i32* %retval, align 4
  store i32 1, i32* %cleanup.dest.slot, align 4
  br label %cleanup

if.end:                                           ; preds = %for.end
  %10 = load i64, i64* %stoneSteps, align 8, !tbaa !2
  %call5 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([18 x i8], [18 x i8]* @.str, i64 0, i64 0), i64 %10)
  store i32 1, i32* %retval, align 4
  store i32 1, i32* %cleanup.dest.slot, align 4
  br label %cleanup

cleanup:                                          ; preds = %if.end, %if.then
  %11 = bitcast i32* %i to i8*
  call void @llvm.lifetime.end.p0i8(i64 4, i8* %11) #5
  %12 = bitcast i64* %stoneSteps to i8*
  call void @llvm.lifetime.end.p0i8(i64 8, i8* %12) #5
  %13 = bitcast i64* %ans to i8*
  call void @llvm.lifetime.end.p0i8(i64 8, i8* %13) #5
  %14 = load i32, i32* %retval, align 4
  ret i32 %14
}

declare dso_local i32 @printf(i8*, ...) #4

attributes #0 = { noinline uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="none" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" "xray-instruction-threshold"="1" }
attributes #1 = { noinline nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="none" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" "xray-instruction-threshold"="1" }
attributes #2 = { argmemonly nounwind willreturn }
attributes #3 = { norecurse uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="none" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" "xray-instruction-threshold"="1" }
attributes #4 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="none" "less-precise-fpmad"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #5 = { nounwind }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 10.0.0 (git@github.com:halo-project/llvm.git b3a4a5ce2a46a6fbd8fde33377366bf8f02eda6f)"}
!2 = !{!3, !3, i64 0}
!3 = !{!"long", !4, i64 0}
!4 = !{!"omnipotent char", !5, i64 0}
!5 = !{!"Simple C++ TBAA"}
!6 = !{!7, !7, i64 0}
!7 = !{!"int", !4, i64 0}

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

@.str = private unnamed_addr constant [18 x i8] c"stoneSteps = %lu\0A\00", align 1

;;;;;; IR Dump After halo::ExternalizeGlobalsPass ;;;;;;
source_filename = "basic/linear_hot.cpp"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@.str = private unnamed_addr constant [18 x i8] c"stoneSteps = %lu\0A\00", align 1

; Function Attrs: noinline uwtable
define dso_local i64 @_Z8fib_leftm(i64 %n) #0 {
entry:
  %retval = alloca i64, align 8
  %n.addr = alloca i64, align 8
  store i64 %n, i64* %n.addr, align 8, !tbaa !2
  %0 = load i64, i64* %n.addr, align 8, !tbaa !2
  %cmp = icmp ult i64 %0, 2
  br i1 %cmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  %1 = load i64, i64* %n.addr, align 8, !tbaa !2
  store i64 %1, i64* %retval, align 8
  br label %return

if.end:                                           ; preds = %entry
  %2 = load i64, i64* %n.addr, align 8, !tbaa !2
  %sub = sub i64 %2, 1
  %call = call i64 @_Z8fib_leftm(i64 %sub)
  %3 = load i64, i64* %n.addr, align 8, !tbaa !2
  %sub1 = sub i64 %3, 2
  %call2 = call i64 @_Z9fib_rightm(i64 %sub1)
  %add = add i64 %call, %call2
  store i64 %add, i64* %retval, align 8
  br label %return

return:                                           ; preds = %if.end, %if.then
  %4 = load i64, i64* %retval, align 8
  ret i64 %4
}

; Function Attrs: noinline uwtable
define dso_local i64 @_Z9fib_rightm(i64 %n) #0 {
entry:
  %retval = alloca i64, align 8
  %n.addr = alloca i64, align 8
  store i64 %n, i64* %n.addr, align 8, !tbaa !2
  %0 = load i64, i64* %n.addr, align 8, !tbaa !2
  %cmp = icmp ult i64 %0, 2
  br i1 %cmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  %1 = load i64, i64* %n.addr, align 8, !tbaa !2
  store i64 %1, i64* %retval, align 8
  br label %return

if.end:                                           ; preds = %entry
  %2 = load i64, i64* %n.addr, align 8, !tbaa !2
  %sub = sub i64 %2, 1
  %call = call i64 @_Z8fib_leftm(i64 %sub)
  %3 = load i64, i64* %n.addr, align 8, !tbaa !2
  %sub1 = sub i64 %3, 2
  %call2 = call i64 @_Z9fib_rightm(i64 %sub1)
  %add = add i64 %call, %call2
  store i64 %add, i64* %retval, align 8
  br label %return

return:                                           ; preds = %if.end, %if.then
  %4 = load i64, i64* %retval, align 8
  ret i64 %4
}

; Function Attrs: noinline uwtable
define dso_local i64 @_Z3fibm(i64 %n) #0 {
entry:
  %n.addr = alloca i64, align 8
  store i64 %n, i64* %n.addr, align 8, !tbaa !2
  %0 = load i64, i64* %n.addr, align 8, !tbaa !2
  %call = call i64 @_Z8fib_leftm(i64 %0)
  ret i64 %call
}

; Function Attrs: noinline nounwind uwtable
define dso_local i64 @_Z17compute_hailstonel(i64 %limit) #1 {
entry:
  %limit.addr = alloca i64, align 8
  %x = alloca i64, align 8
  %reachedOne = alloca i64, align 8
  %totalSteps = alloca i64, align 8
  store i64 %limit, i64* %limit.addr, align 8, !tbaa !2
  %0 = bitcast i64* %x to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* %0) #5
  store i64 27, i64* %x, align 8, !tbaa !2
  %1 = bitcast i64* %reachedOne to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* %1) #5
  store i64 0, i64* %reachedOne, align 8, !tbaa !2
  %2 = bitcast i64* %totalSteps to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* %2) #5
  store i64 0, i64* %totalSteps, align 8, !tbaa !2
  br label %while.cond

while.cond:                                       ; preds = %if.end6, %entry
  %3 = load i64, i64* %reachedOne, align 8, !tbaa !2
  %4 = load i64, i64* %limit.addr, align 8, !tbaa !2
  %cmp = icmp slt i64 %3, %4
  br i1 %cmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.cond
  %5 = load i64, i64* %x, align 8, !tbaa !2
  %cmp1 = icmp eq i64 %5, 1
  br i1 %cmp1, label %if.then, label %if.end

if.then:                                          ; preds = %while.body
  %6 = load i64, i64* %reachedOne, align 8, !tbaa !2
  %add = add nsw i64 27, %6
  store i64 %add, i64* %x, align 8, !tbaa !2
  %7 = load i64, i64* %reachedOne, align 8, !tbaa !2
  %inc = add nsw i64 %7, 1
  store i64 %inc, i64* %reachedOne, align 8, !tbaa !2
  br label %if.end

if.end:                                           ; preds = %if.then, %while.body
  %8 = load i64, i64* %totalSteps, align 8, !tbaa !2
  %inc2 = add nsw i64 %8, 1
  store i64 %inc2, i64* %totalSteps, align 8, !tbaa !2
  %9 = load i64, i64* %x, align 8, !tbaa !2
  %rem = srem i64 %9, 2
  %cmp3 = icmp eq i64 %rem, 0
  br i1 %cmp3, label %if.then4, label %if.else

if.then4:                                         ; preds = %if.end
  %10 = load i64, i64* %x, align 8, !tbaa !2
  %div = sdiv i64 %10, 2
  store i64 %div, i64* %x, align 8, !tbaa !2
  br label %if.end6

if.else:                                          ; preds = %if.end
  %11 = load i64, i64* %x, align 8, !tbaa !2
  %mul = mul nsw i64 3, %11
  %add5 = add nsw i64 %mul, 1
  store i64 %add5, i64* %x, align 8, !tbaa !2
  br label %if.end6

if.end6:                                          ; preds = %if.else, %if.then4
  br label %while.cond

while.end:                                        ; preds = %while.cond
  %12 = load i64, i64* %totalSteps, align 8, !tbaa !2
  %13 = bitcast i64* %totalSteps to i8*
  call void @llvm.lifetime.end.p0i8(i64 8, i8* %13) #5
  %14 = bitcast i64* %reachedOne to i8*
  call void @llvm.lifetime.end.p0i8(i64 8, i8* %14) #5
  %15 = bitcast i64* %x to i8*
  call void @llvm.lifetime.end.p0i8(i64 8, i8* %15) #5
  ret i64 %12
}

; Function Attrs: argmemonly nounwind willreturn
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #2

; Function Attrs: argmemonly nounwind willreturn
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #2

; Function Attrs: norecurse uwtable
define dso_local i32 @main() #3 {
entry:
  %retval = alloca i32, align 4
  %ans = alloca i64, align 8
  %stoneSteps = alloca i64, align 8
  %i = alloca i32, align 4
  %cleanup.dest.slot = alloca i32, align 4
  store i32 0, i32* %retval, align 4
  %0 = bitcast i64* %ans to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* %0) #5
  store i64 0, i64* %ans, align 8, !tbaa !2
  %1 = bitcast i64* %stoneSteps to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* %1) #5
  store i64 0, i64* %stoneSteps, align 8, !tbaa !2
  %2 = bitcast i32* %i to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* %2) #5
  store i32 -4, i32* %i, align 4, !tbaa !6
  br label %for.cond

for.cond:                                         ; preds = %for.inc, %entry
  %3 = load i32, i32* %i, align 4, !tbaa !6
  %cmp = icmp slt i32 %3, 10
  br i1 %cmp, label %for.body, label %for.end

for.body:                                         ; preds = %for.cond
  %4 = load i32, i32* %i, align 4, !tbaa !6
  %mul = mul nsw i32 500000, %4
  %conv = sext i32 %mul to i64
  %call = call i64 @_Z17compute_hailstonel(i64 %conv)
  %5 = load i64, i64* %stoneSteps, align 8, !tbaa !2
  %add = add i64 %5, %call
  store i64 %add, i64* %stoneSteps, align 8, !tbaa !2
  %call1 = call i64 @_Z3fibm(i64 40)
  %6 = load i64, i64* %ans, align 8, !tbaa !2
  %add2 = add i64 %6, %call1
  store i64 %add2, i64* %ans, align 8, !tbaa !2
  br label %for.inc

for.inc:                                          ; preds = %for.body
  %7 = load i32, i32* %i, align 4, !tbaa !6
  %inc = add nsw i32 %7, 1
  store i32 %inc, i32* %i, align 4, !tbaa !6
  br label %for.cond

for.end:                                          ; preds = %for.cond
  %8 = load i64, i64* %ans, align 8, !tbaa !2
  %9 = load i32, i32* %i, align 4, !tbaa !6
  %conv3 = sext i32 %9 to i64
  %rem = urem i64 %8, %conv3
  %cmp4 = icmp eq i64 %rem, 0
  br i1 %cmp4, label %if.then, label %if.end

if.then:                                          ; preds = %for.end
  store i32 0, i32* %retval, align 4
  store i32 1, i32* %cleanup.dest.slot, align 4
  br label %cleanup

if.end:                                           ; preds = %for.end
  %10 = load i64, i64* %stoneSteps, align 8, !tbaa !2
  %call5 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([18 x i8], [18 x i8]* @.str, i64 0, i64 0), i64 %10)
  store i32 1, i32* %retval, align 4
  store i32 1, i32* %cleanup.dest.slot, align 4
  br label %cleanup

cleanup:                                          ; preds = %if.end, %if.then
  %11 = bitcast i32* %i to i8*
  call void @llvm.lifetime.end.p0i8(i64 4, i8* %11) #5
  %12 = bitcast i64* %stoneSteps to i8*
  call void @llvm.lifetime.end.p0i8(i64 8, i8* %12) #5
  %13 = bitcast i64* %ans to i8*
  call void @llvm.lifetime.end.p0i8(i64 8, i8* %13) #5
  %14 = load i32, i32* %retval, align 4
  ret i32 %14
}

declare dso_local i32 @printf(i8*, ...) #4

attributes #0 = { noinline uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="none" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" "xray-instruction-threshold"="1" }
attributes #1 = { noinline nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="none" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" "xray-instruction-threshold"="1" }
attributes #2 = { argmemonly nounwind willreturn }
attributes #3 = { norecurse uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="none" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" "xray-instruction-threshold"="1" }
attributes #4 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="none" "less-precise-fpmad"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #5 = { nounwind }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 10.0.0 (git@github.com:halo-project/llvm.git b3a4a5ce2a46a6fbd8fde33377366bf8f02eda6f)"}
!2 = !{!3, !3, i64 0}
!3 = !{!"long", !4, i64 0}
!4 = !{!"omnipotent char", !5, i64 0}
!5 = !{!"Simple C++ TBAA"}
!6 = !{!7, !7, i64 0}
!7 = !{!"int", !4, i64 0}

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;


;;;;;; IR Dump After ModuleToFunctionPassAdaptor<llvm::FunctionToLoopPassAdaptor<halo::LoopNamerPass> > ;;;;;;
source_filename = "basic/linear_hot.cpp"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@.str = private unnamed_addr constant [18 x i8] c"stoneSteps = %lu\0A\00", align 1

; Function Attrs: noinline uwtable
define dso_local i64 @_Z8fib_leftm(i64 %n) #0 {
entry:
  %retval = alloca i64, align 8
  %n.addr = alloca i64, align 8
  store i64 %n, i64* %n.addr, align 8, !tbaa !2
  %0 = load i64, i64* %n.addr, align 8, !tbaa !2
  %cmp = icmp ult i64 %0, 2
  br i1 %cmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  %1 = load i64, i64* %n.addr, align 8, !tbaa !2
  store i64 %1, i64* %retval, align 8
  br label %return

if.end:                                           ; preds = %entry
  %2 = load i64, i64* %n.addr, align 8, !tbaa !2
  %sub = sub i64 %2, 1
  %call = call i64 @_Z8fib_leftm(i64 %sub)
  %3 = load i64, i64* %n.addr, align 8, !tbaa !2
  %sub1 = sub i64 %3, 2
  %call2 = call i64 @_Z9fib_rightm(i64 %sub1)
  %add = add i64 %call, %call2
  store i64 %add, i64* %retval, align 8
  br label %return

return:                                           ; preds = %if.end, %if.then
  %4 = load i64, i64* %retval, align 8
  ret i64 %4
}

; Function Attrs: noinline uwtable
define dso_local i64 @_Z9fib_rightm(i64 %n) #0 {
entry:
  %retval = alloca i64, align 8
  %n.addr = alloca i64, align 8
  store i64 %n, i64* %n.addr, align 8, !tbaa !2
  %0 = load i64, i64* %n.addr, align 8, !tbaa !2
  %cmp = icmp ult i64 %0, 2
  br i1 %cmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  %1 = load i64, i64* %n.addr, align 8, !tbaa !2
  store i64 %1, i64* %retval, align 8
  br label %return

if.end:                                           ; preds = %entry
  %2 = load i64, i64* %n.addr, align 8, !tbaa !2
  %sub = sub i64 %2, 1
  %call = call i64 @_Z8fib_leftm(i64 %sub)
  %3 = load i64, i64* %n.addr, align 8, !tbaa !2
  %sub1 = sub i64 %3, 2
  %call2 = call i64 @_Z9fib_rightm(i64 %sub1)
  %add = add i64 %call, %call2
  store i64 %add, i64* %retval, align 8
  br label %return

return:                                           ; preds = %if.end, %if.then
  %4 = load i64, i64* %retval, align 8
  ret i64 %4
}

; Function Attrs: noinline uwtable
define dso_local i64 @_Z3fibm(i64 %n) #0 {
entry:
  %n.addr = alloca i64, align 8
  store i64 %n, i64* %n.addr, align 8, !tbaa !2
  %0 = load i64, i64* %n.addr, align 8, !tbaa !2
  %call = call i64 @_Z8fib_leftm(i64 %0)
  ret i64 %call
}

; Function Attrs: noinline nounwind uwtable
define dso_local i64 @_Z17compute_hailstonel(i64 %limit) #1 {
entry:
  %limit.addr = alloca i64, align 8
  %x = alloca i64, align 8
  %reachedOne = alloca i64, align 8
  %totalSteps = alloca i64, align 8
  store i64 %limit, i64* %limit.addr, align 8, !tbaa !2
  %0 = bitcast i64* %x to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* %0) #5
  store i64 27, i64* %x, align 8, !tbaa !2
  %1 = bitcast i64* %reachedOne to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* %1) #5
  store i64 0, i64* %reachedOne, align 8, !tbaa !2
  %2 = bitcast i64* %totalSteps to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* %2) #5
  store i64 0, i64* %totalSteps, align 8, !tbaa !2
  br label %while.cond

while.cond:                                       ; preds = %if.end6, %entry
  %3 = load i64, i64* %reachedOne, align 8, !tbaa !2
  %4 = load i64, i64* %limit.addr, align 8, !tbaa !2
  %cmp = icmp slt i64 %3, %4
  br i1 %cmp, label %while.body, label %while.end

while.body:                                       ; preds = %while.cond
  %5 = load i64, i64* %x, align 8, !tbaa !2
  %cmp1 = icmp eq i64 %5, 1
  br i1 %cmp1, label %if.then, label %if.end

if.then:                                          ; preds = %while.body
  %6 = load i64, i64* %reachedOne, align 8, !tbaa !2
  %add = add nsw i64 27, %6
  store i64 %add, i64* %x, align 8, !tbaa !2
  %7 = load i64, i64* %reachedOne, align 8, !tbaa !2
  %inc = add nsw i64 %7, 1
  store i64 %inc, i64* %reachedOne, align 8, !tbaa !2
  br label %if.end

if.end:                                           ; preds = %if.then, %while.body
  %8 = load i64, i64* %totalSteps, align 8, !tbaa !2
  %inc2 = add nsw i64 %8, 1
  store i64 %inc2, i64* %totalSteps, align 8, !tbaa !2
  %9 = load i64, i64* %x, align 8, !tbaa !2
  %rem = srem i64 %9, 2
  %cmp3 = icmp eq i64 %rem, 0
  br i1 %cmp3, label %if.then4, label %if.else

if.then4:                                         ; preds = %if.end
  %10 = load i64, i64* %x, align 8, !tbaa !2
  %div = sdiv i64 %10, 2
  store i64 %div, i64* %x, align 8, !tbaa !2
  br label %if.end6

if.else:                                          ; preds = %if.end
  %11 = load i64, i64* %x, align 8, !tbaa !2
  %mul = mul nsw i64 3, %11
  %add5 = add nsw i64 %mul, 1
  store i64 %add5, i64* %x, align 8, !tbaa !2
  br label %if.end6

if.end6:                                          ; preds = %if.else, %if.then4
  br label %while.cond, !llvm.loop !6

while.end:                                        ; preds = %while.cond
  %12 = load i64, i64* %totalSteps, align 8, !tbaa !2
  %13 = bitcast i64* %totalSteps to i8*
  call void @llvm.lifetime.end.p0i8(i64 8, i8* %13) #5
  %14 = bitcast i64* %reachedOne to i8*
  call void @llvm.lifetime.end.p0i8(i64 8, i8* %14) #5
  %15 = bitcast i64* %x to i8*
  call void @llvm.lifetime.end.p0i8(i64 8, i8* %15) #5
  ret i64 %12
}

; Function Attrs: argmemonly nounwind willreturn
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #2

; Function Attrs: argmemonly nounwind willreturn
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #2

; Function Attrs: norecurse uwtable
define dso_local i32 @main() #3 {
entry:
  %retval = alloca i32, align 4
  %ans = alloca i64, align 8
  %stoneSteps = alloca i64, align 8
  %i = alloca i32, align 4
  %cleanup.dest.slot = alloca i32, align 4
  store i32 0, i32* %retval, align 4
  %0 = bitcast i64* %ans to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* %0) #5
  store i64 0, i64* %ans, align 8, !tbaa !2
  %1 = bitcast i64* %stoneSteps to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* %1) #5
  store i64 0, i64* %stoneSteps, align 8, !tbaa !2
  %2 = bitcast i32* %i to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* %2) #5
  store i32 -4, i32* %i, align 4, !tbaa !8
  br label %for.cond

for.cond:                                         ; preds = %for.inc, %entry
  %3 = load i32, i32* %i, align 4, !tbaa !8
  %cmp = icmp slt i32 %3, 10
  br i1 %cmp, label %for.body, label %for.end

for.body:                                         ; preds = %for.cond
  %4 = load i32, i32* %i, align 4, !tbaa !8
  %mul = mul nsw i32 500000, %4
  %conv = sext i32 %mul to i64
  %call = call i64 @_Z17compute_hailstonel(i64 %conv)
  %5 = load i64, i64* %stoneSteps, align 8, !tbaa !2
  %add = add i64 %5, %call
  store i64 %add, i64* %stoneSteps, align 8, !tbaa !2
  %call1 = call i64 @_Z3fibm(i64 40)
  %6 = load i64, i64* %ans, align 8, !tbaa !2
  %add2 = add i64 %6, %call1
  store i64 %add2, i64* %ans, align 8, !tbaa !2
  br label %for.inc

for.inc:                                          ; preds = %for.body
  %7 = load i32, i32* %i, align 4, !tbaa !8
  %inc = add nsw i32 %7, 1
  store i32 %inc, i32* %i, align 4, !tbaa !8
  br label %for.cond, !llvm.loop !10

for.end:                                          ; preds = %for.cond
  %8 = load i64, i64* %ans, align 8, !tbaa !2
  %9 = load i32, i32* %i, align 4, !tbaa !8
  %conv3 = sext i32 %9 to i64
  %rem = urem i64 %8, %conv3
  %cmp4 = icmp eq i64 %rem, 0
  br i1 %cmp4, label %if.then, label %if.end

if.then:                                          ; preds = %for.end
  store i32 0, i32* %retval, align 4
  store i32 1, i32* %cleanup.dest.slot, align 4
  br label %cleanup

if.end:                                           ; preds = %for.end
  %10 = load i64, i64* %stoneSteps, align 8, !tbaa !2
  %call5 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([18 x i8], [18 x i8]* @.str, i64 0, i64 0), i64 %10)
  store i32 1, i32* %retval, align 4
  store i32 1, i32* %cleanup.dest.slot, align 4
  br label %cleanup

cleanup:                                          ; preds = %if.end, %if.then
  %11 = bitcast i32* %i to i8*
  call void @llvm.lifetime.end.p0i8(i64 4, i8* %11) #5
  %12 = bitcast i64* %stoneSteps to i8*
  call void @llvm.lifetime.end.p0i8(i64 8, i8* %12) #5
  %13 = bitcast i64* %ans to i8*
  call void @llvm.lifetime.end.p0i8(i64 8, i8* %13) #5
  %14 = load i32, i32* %retval, align 4
  ret i32 %14
}

declare dso_local i32 @printf(i8*, ...) #4

attributes #0 = { noinline uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="none" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" "xray-instruction-threshold"="1" }
attributes #1 = { noinline nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="none" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" "xray-instruction-threshold"="1" }
attributes #2 = { argmemonly nounwind willreturn }
attributes #3 = { norecurse uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="none" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" "xray-instruction-threshold"="1" }
attributes #4 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="none" "less-precise-fpmad"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #5 = { nounwind }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 10.0.0 (git@github.com:halo-project/llvm.git b3a4a5ce2a46a6fbd8fde33377366bf8f02eda6f)"}
!2 = !{!3, !3, i64 0}
!3 = !{!"long", !4, i64 0}
!4 = !{!"omnipotent char", !5, i64 0}
!5 = !{!"Simple C++ TBAA"}
!6 = distinct !{!6, !7}
!7 = !{!"llvm.loop.id", !"0"}
!8 = !{!9, !9, i64 0}
!9 = !{!"int", !4, i64 0}
!10 = distinct !{!10, !11}
!11 = !{!"llvm.loop.id", !"1"}

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;


;;;;;; IR Dump At after optimization pipeline. ;;;;;;
source_filename = "basic/linear_hot.cpp"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@.str = private unnamed_addr constant [18 x i8] c"stoneSteps = %lu\0A\00", align 1

; Function Attrs: noinline nounwind readnone uwtable
define dso_local i64 @_Z8fib_leftm(i64 %n) local_unnamed_addr #0 {
entry:
  %cmp = icmp ult i64 %n, 2
  br i1 %cmp, label %return, label %if.end

if.end:                                           ; preds = %entry
  %sub1 = add i64 %n, -2
  %sub = add i64 %n, -1
  %call = tail call i64 @_Z8fib_leftm(i64 %sub)
  %call2 = tail call i64 @_Z9fib_rightm(i64 %sub1)
  %add = add i64 %call2, %call
  ret i64 %add

return:                                           ; preds = %entry
  ret i64 %n
}

; Function Attrs: noinline nounwind readnone uwtable
define dso_local i64 @_Z9fib_rightm(i64 %n) local_unnamed_addr #0 {
entry:
  %cmp = icmp ult i64 %n, 2
  br i1 %cmp, label %return, label %if.end

if.end:                                           ; preds = %entry
  %sub1 = add i64 %n, -2
  %sub = add i64 %n, -1
  %call = tail call i64 @_Z8fib_leftm(i64 %sub)
  %call2 = tail call i64 @_Z9fib_rightm(i64 %sub1)
  %add = add i64 %call2, %call
  ret i64 %add

return:                                           ; preds = %entry
  ret i64 %n
}

; Function Attrs: nofree noinline nounwind uwtable
define dso_local i64 @_Z3fibm(i64 %n) local_unnamed_addr #1 {
entry:
  %call = tail call i64 @_Z8fib_leftm(i64 %n)
  ret i64 %call
}

; Function Attrs: noinline norecurse nounwind readnone uwtable
define dso_local i64 @_Z17compute_hailstonel(i64 %limit) local_unnamed_addr #2 {
entry:
  %cmp9 = icmp sgt i64 %limit, 0
  br i1 %cmp9, label %while.body, label %while.end

while.body:                                       ; preds = %entry, %while.body
  %totalSteps.012 = phi i64 [ %inc2, %while.body ], [ 0, %entry ]
  %reachedOne.011 = phi i64 [ %reachedOne.1, %while.body ], [ 0, %entry ]
  %x.010 = phi i64 [ %x.2, %while.body ], [ 27, %entry ]
  %cmp1 = icmp eq i64 %x.010, 1
  %add = add nuw nsw i64 %reachedOne.011, 27
  %x.1 = select i1 %cmp1, i64 %add, i64 %x.010
  %inc = zext i1 %cmp1 to i64
  %reachedOne.1 = add nuw nsw i64 %reachedOne.011, %inc
  %inc2 = add nuw nsw i64 %totalSteps.012, 1
  %0 = and i64 %x.1, 1
  %cmp3 = icmp eq i64 %0, 0
  %div = sdiv i64 %x.1, 2
  %mul = mul nsw i64 %x.1, 3
  %add5 = add nsw i64 %mul, 1
  %x.2 = select i1 %cmp3, i64 %div, i64 %add5
  %cmp = icmp slt i64 %reachedOne.1, %limit
  br i1 %cmp, label %while.body, label %while.end, !llvm.loop !2

while.end:                                        ; preds = %while.body, %entry
  %totalSteps.0.lcssa = phi i64 [ 0, %entry ], [ %inc2, %while.body ]
  ret i64 %totalSteps.0.lcssa
}

; Function Attrs: nofree norecurse nounwind uwtable
define dso_local i32 @main() local_unnamed_addr #3 {
entry:
  %call1 = tail call i64 @_Z3fibm(i64 40)
  %call1.1 = tail call i64 @_Z3fibm(i64 40)
  %add2.1 = add i64 %call1.1, %call1
  %call1.2 = tail call i64 @_Z3fibm(i64 40)
  %add2.2 = add i64 %call1.2, %add2.1
  %call1.3 = tail call i64 @_Z3fibm(i64 40)
  %add2.3 = add i64 %call1.3, %add2.2
  %call1.4 = tail call i64 @_Z3fibm(i64 40)
  %add2.4 = add i64 %call1.4, %add2.3
  %call1.5 = tail call i64 @_Z3fibm(i64 40)
  %add2.5 = add i64 %call1.5, %add2.4
  %call1.6 = tail call i64 @_Z3fibm(i64 40)
  %add2.6 = add i64 %call1.6, %add2.5
  %call1.7 = tail call i64 @_Z3fibm(i64 40)
  %add2.7 = add i64 %call1.7, %add2.6
  %call1.8 = tail call i64 @_Z3fibm(i64 40)
  %add2.8 = add i64 %call1.8, %add2.7
  %call1.9 = tail call i64 @_Z3fibm(i64 40)
  %add2.9 = add i64 %call1.9, %add2.8
  %call1.10 = tail call i64 @_Z3fibm(i64 40)
  %add2.10 = add i64 %call1.10, %add2.9
  %call1.11 = tail call i64 @_Z3fibm(i64 40)
  %add2.11 = add i64 %call1.11, %add2.10
  %call1.12 = tail call i64 @_Z3fibm(i64 40)
  %add2.12 = add i64 %call1.12, %add2.11
  %call1.13 = tail call i64 @_Z3fibm(i64 40)
  %add2.13 = add i64 %call1.13, %add2.12
  %rem = urem i64 %add2.13, 10
  %cmp4 = icmp eq i64 %rem, 0
  br i1 %cmp4, label %cleanup, label %if.end

if.end:                                           ; preds = %entry
  %call.13 = tail call i64 @_Z17compute_hailstonel(i64 4500000)
  %call.12 = tail call i64 @_Z17compute_hailstonel(i64 4000000)
  %call.11 = tail call i64 @_Z17compute_hailstonel(i64 3500000)
  %call.10 = tail call i64 @_Z17compute_hailstonel(i64 3000000)
  %call.9 = tail call i64 @_Z17compute_hailstonel(i64 2500000)
  %call.8 = tail call i64 @_Z17compute_hailstonel(i64 2000000)
  %call.7 = tail call i64 @_Z17compute_hailstonel(i64 1500000)
  %call.6 = tail call i64 @_Z17compute_hailstonel(i64 1000000)
  %call.5 = tail call i64 @_Z17compute_hailstonel(i64 500000)
  %call.4 = tail call i64 @_Z17compute_hailstonel(i64 0)
  %call.3 = tail call i64 @_Z17compute_hailstonel(i64 -500000)
  %call.2 = tail call i64 @_Z17compute_hailstonel(i64 -1000000)
  %call.1 = tail call i64 @_Z17compute_hailstonel(i64 -1500000)
  %call = tail call i64 @_Z17compute_hailstonel(i64 -2000000)
  %add.1 = add i64 %call.1, %call
  %add.2 = add i64 %call.2, %add.1
  %add.3 = add i64 %call.3, %add.2
  %add.4 = add i64 %call.4, %add.3
  %add.5 = add i64 %call.5, %add.4
  %add.6 = add i64 %call.6, %add.5
  %add.7 = add i64 %call.7, %add.6
  %add.8 = add i64 %call.8, %add.7
  %add.9 = add i64 %call.9, %add.8
  %add.10 = add i64 %call.10, %add.9
  %add.11 = add i64 %call.11, %add.10
  %add.12 = add i64 %call.12, %add.11
  %add.13 = add i64 %call.13, %add.12
  %call5 = tail call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([18 x i8], [18 x i8]* @.str, i64 0, i64 0), i64 %add.13)
  br label %cleanup

cleanup:                                          ; preds = %entry, %if.end
  %retval.0 = phi i32 [ 1, %if.end ], [ 0, %entry ]
  ret i32 %retval.0
}

; Function Attrs: nofree nounwind
declare dso_local i32 @printf(i8* nocapture readonly, ...) local_unnamed_addr #4

attributes #0 = { noinline nounwind readnone uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="none" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" "xray-instruction-threshold"="1" }
attributes #1 = { nofree noinline nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="none" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" "xray-instruction-threshold"="1" }
attributes #2 = { noinline norecurse nounwind readnone uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="none" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" "xray-instruction-threshold"="1" }
attributes #3 = { nofree norecurse nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="none" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" "xray-instruction-threshold"="1" }
attributes #4 = { nofree nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="none" "less-precise-fpmad"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 10.0.0 (git@github.com:halo-project/llvm.git b3a4a5ce2a46a6fbd8fde33377366bf8f02eda6f)"}
!2 = distinct !{!2, !3}
!3 = !{!"llvm.loop.id", !"0"}

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

socket event: End of file
