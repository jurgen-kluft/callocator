# App

Can you write a local MacOS app in Golang.

What are the functionalities that we are looking for:

- Written in Golang with as little dependencies as possible
- Many different small services that can be started and stopped independently
- A supervisor (CLI) to start and stop the different services
- Use gmail OAUTH to send and receive email


## Services

- email-service
    - Send and receive email using gmail OAUTH
    - Process incoming email to trigger actions in other services
- pushover-service
    - Send notifications using the Pushover service
