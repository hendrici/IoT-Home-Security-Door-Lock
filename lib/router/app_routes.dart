import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';
import 'package:lock_app/pages/lock_page.dart';
import 'constants.dart';

class AppRouter {
  static final _rootNavigatorKey = GlobalKey<NavigatorState>();

  static final GoRouter _router = GoRouter(
    initialLocation: Routes.lockPage,
    debugLogDiagnostics: true,
    navigatorKey: _rootNavigatorKey,
    routes: [
      GoRoute(
        path: Routes.lockPage,
        builder: (context, state) => const LockPage(),
      ),
    ],
  );

  static GoRouter get router => _router;
}
